/********************************************************************************************
 *
 *  Command line utility to produce Assembly-spectra plots
 *
 *  Author:  Gene Myers
 *  Date  :  March, 2021
 *
 *********************************************************************************************/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <math.h>

#define DEBUG

#include "libfastk.h"
#include "asm_plotter.h"

static char *Usage[] = { " [-w<double(6.0)>] [-h<double(4.5)>]",
                         " [-[xX]<number(x2.1)>] [-[yY]<number(y1.1)>]",
                         " [-v] [-lfs] [-pdf] [-z] [-T<int(4)>] [-P<dir(/tmp)>]",
                         " <reads>[.ktab] <asm1dna> [<asm2:.dna>] <out>"
                       };

static char template[15] = "._ASM.XXXX";

static int check_table(char *name, int lmer)
{ int   kmer;
  FILE *f;
  
  f = fopen(name,"r");
  if (f == NULL)
    { fprintf(stderr,"%s: Cannot find FastK table %s\n",Prog_Name,name);
      exit (1);
    }
  else
    { fread(&kmer,sizeof(int),1,f);
      if (lmer != 0 && kmer != lmer)
        { fprintf(stderr,"%s: Kmer (%d) of table %s != %d\n",Prog_Name,kmer,name,lmer);
          exit (1);
        }
      fclose(f);
      return (kmer);
    }
}

int main(int argc, char *argv[])
{ int    KMER;
  int    VERBOSE;
  int    LINE, FILL, STACK;
  int    PDF;
  int    ZGRAM;
  double XDIM, YDIM;
  double XREL, YREL;
  int    XMAX;
  int64  YMAX;
  char  *OUT;
  char  *ASM[2];
  char  *READS;
  int    NTHREADS;
  char  *SORT_PATH;
  
  { int    i, j, k;
    int    flags[128];
    char  *eptr;

    (void) flags;

    ARG_INIT("ASMpLot");

    XDIM = 6.0;
    YDIM = 4.5;
    XREL = 2.1;
    YREL = 1.1;
    XMAX = 0;
    YMAX = 0;
    PDF  = 0;
    NTHREADS = 4;
    SORT_PATH = "/tmp";

    j = 1;
    for (i = 1; i < argc; i++)
      if (argv[i][0] == '-')
        switch (argv[i][1])
        { default:
            ARG_FLAGS("vlfsz")
            break;
          case 'h':
            ARG_REAL(YDIM);
            break;
          case 'p':
            if (strcmp("df",argv[i]+2) == 0)
              PDF = 1;
            else
              { fprintf(stderr,"%s: don't recognize option %s\n",Prog_Name,argv[i]);
                exit (1);
              }
            break;
          case 'w':
            ARG_REAL(XDIM);
            break;
          case 'x':
            ARG_REAL(XREL);
            if (XREL <= 0.)
              { fprintf(stderr,"%s: max x scaling factor must be > 0\n",Prog_Name);
                exit (1);
              }
            break;
          case 'y':
            ARG_REAL(YREL);
            if (YREL <= 0.)
              { fprintf(stderr,"%s: max y scaling factor must be > 0\n",Prog_Name);
                exit (1);
              }
            break;
          case 'P':
            SORT_PATH = argv[i]+2;
            break;
          case 'T':
            ARG_POSITIVE(NTHREADS,"Number of threads")
            break;
          case 'X':
            ARG_POSITIVE(XMAX,"x max");
            break;
          case 'Y':
            { int ymax;

               ARG_POSITIVE(ymax,"y max");
               YMAX = ymax;
               break;
            }
        }
      else
        argv[j++] = argv[i];
    argc = j;

    VERBOSE = flags['v'];
    LINE    = flags['l'];
    FILL    = flags['f'];
    STACK   = flags['s'];
    ZGRAM   = flags['z'];

    if (argc != 4 && argc != 5)
      { fprintf(stderr,"\nUsage: %s %s\n",Prog_Name,Usage[0]);
        fprintf(stderr,"       %*s %s\n",(int) strlen(Prog_Name),"",Usage[1]);
        fprintf(stderr,"       %*s %s\n",(int) strlen(Prog_Name),"",Usage[2]);
        fprintf(stderr,"       %*s %s\n",(int) strlen(Prog_Name),"",Usage[3]);
        fprintf(stderr,"\n");
        fprintf(stderr,"      -w: width in inches of plots\n");
        fprintf(stderr,"      -h: height in inches of plots\n");
        fprintf(stderr,"      -x: max x as a real-valued multiple of x* with max\n");
        fprintf(stderr,"              count 'peak' away from the origin\n");
        fprintf(stderr,"      -X: max x as an int value in absolute terms\n");
        fprintf(stderr,"      -y: max y as a real-valued multiple of max count\n");
        fprintf(stderr,"              'peak' away from the origin\n");
        fprintf(stderr,"      -Y: max y as an int value in absolute terms\n");
        fprintf(stderr,"\n");
        fprintf(stderr,"      -l: draw line plot\n");
        fprintf(stderr,"      -f: draw fill plot\n");
        fprintf(stderr,"      -s: draw stack plot\n");
        fprintf(stderr,"          any combo allowed, none => draw all\n");
        fprintf(stderr,"\n");
	fprintf(stderr,"    -pdf: output .pdf (default is .png)\n");
        fprintf(stderr,"\n");
        fprintf(stderr,"      -z: plot counts of k-mers unique to one or both assemblies\n");
        fprintf(stderr,"\n");
        fprintf(stderr,"      -v: verbose output to stderr\n");
        fprintf(stderr,"      -T: number of threads to use\n");
        fprintf(stderr,"      -P: Place all temporary files in directory -P.\n");
        exit (1);
      }

    if (LINE+FILL+STACK == 0)
      LINE = FILL = STACK = 1;
    READS  = argv[1];
    ASM[0] = argv[2];
    if (argc == 5)
      ASM[1] = argv[3];
    else
      ASM[1] = NULL;
    OUT = argv[argc-1];
  }

  { char *troot;
    char *suffix[9] = { ".gz", ".fa", ".fq", ".fasta", ".fastq", ".db", ".sam", ".bam", ".cram" };
    char  command[5000];
    int   i, j, len;

    troot = mktemp(template);

    READS = Root(READS,".ktab");

    KMER = check_table(Catenate(READS,".ktab","",""),0);

    for (i = 0; i < 2; i++)
      { if (ASM[i] == NULL)
          continue;

        for (j = 0; j < 9; j++)
          { len = strlen(ASM[i]) - strlen(suffix[j]);
            if (strcmp(ASM[i]+len,suffix[j]) == 0)
              ASM[i][len] = '\0';
          }

        if (VERBOSE)
          fprintf(stderr,"\n  Making k-mer table for assembly %s\n",ASM[i]);

        sprintf(command,"FastK -k%d -T%d -P%s -t1 %s",KMER,NTHREADS,SORT_PATH,ASM[i]);
        system(command);
      }

    if (VERBOSE)
      fprintf(stderr,"\n  Making Venn histograms and plotting\n");

    asm_plot(OUT,ASM[0],ASM[1],READS,XDIM,YDIM,XREL,YREL,XMAX,YMAX,
             PDF,ZGRAM,LINE,FILL,STACK,troot,NTHREADS);

    for (i = 0; i < 2; i++)
      { if (ASM[i] == NULL)
          continue;
        sprintf(command,"Fastrm %s",ASM[i]);
        system(command);
      }

    free(READS);
  }

  Catenate(NULL,NULL,NULL,NULL);
  Numbered_Suffix(NULL,0,NULL);
  free(Prog_Name);

  exit (0);
}
