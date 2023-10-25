#include <fcntl.h>
#include <stdbool.h>
#include "stdckdint.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

/* A process table entry.  */
struct process
{
  long pid;
  long arrival_time;
  long burst_time;

  TAILQ_ENTRY (process) pointers;

  /* Additional fields here */
  long remaining_time;
  long completion_time;
  long cpu_consumption;
  bool first_queue;
  bool first_run;
  /* End of "Additional fields here" */
};


TAILQ_HEAD (process_list, process);

/* Skip past initial nondigits in *DATA, then scan an unsigned decimal
   integer and return its value.  Do not scan past DATA_END.  Return
   the integer’s value.  Report an error and exit if no integer is
   found, or if the integer overflows.  */
static long
next_int (char const **data, char const *data_end)
{
  long current = 0;
  bool int_start = false;
  char const *d;

  for (d = *data; d < data_end; d++)
    {
      char c = *d;
      if ('0' <= c && c <= '9')
	{
	  int_start = true;
	  if (ckd_mul (&current, current, 10)
	      || ckd_add (&current, current, c - '0'))
	    {
	      fprintf (stderr, "integer overflow\n");
	      exit (1);
	    }
	}
      else if (int_start)
	break;
    }

  if (!int_start)
    {
      fprintf (stderr, "missing integer\n");
      exit (1);
    }

  *data = d;
  return current;
}

/* Return the first unsigned decimal integer scanned from DATA.
   Report an error and exit if no integer is found, or if it overflows.  */
static long
next_int_from_c_str (char const *data)
{
  return next_int (&data, strchr (data, 0));
}

/* A vector of processes of length NPROCESSES; the vector consists of
   PROCESS[0], ..., PROCESS[NPROCESSES - 1].  */
struct process_set
{
  long nprocesses;
  struct process *process;
};

/* Return a vector of processes scanned from the file named FILENAME.
   Report an error and exit on failure.  */
static struct process_set
init_processes (char const *filename)
{
  int fd = open (filename, O_RDONLY);
  if (fd < 0)
    {
      perror ("open");
      exit (1);
    }

  struct stat st;
  if (fstat (fd, &st) < 0)
    {
      perror ("stat");
      exit (1);
    }

  size_t size;
  if (ckd_add (&size, st.st_size, 0))
    {
      fprintf (stderr, "%s: file size out of range\n", filename);
      exit (1);
    }

  char *data_start = mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_start == MAP_FAILED)
    {
      perror ("mmap");
      exit (1);
    }

  char const *data_end = data_start + size;
  char const *data = data_start;

  long nprocesses = next_int (&data, data_end);
  if (nprocesses <= 0)
    {
      fprintf (stderr, "no processes\n");
      exit (1);
    }

  struct process *process = calloc (sizeof *process, nprocesses);
  if (!process)
    {
      perror ("calloc");
      exit (1);
    }

  for (long i = 0; i < nprocesses; i++)
    {
      process[i].pid = next_int (&data, data_end);
      process[i].arrival_time = next_int (&data, data_end);
      process[i].burst_time = next_int (&data, data_end);
      if (process[i].burst_time == 0)
	{
	  fprintf (stderr, "process %ld has zero burst time\n",
		   process[i].pid);
	  exit (1);
	}
    }

  if (munmap (data_start, size) < 0)
    {
      perror ("munmap");
      exit (1);
    }
  if (close (fd) < 0)
    {
      perror ("close");
      exit (1);
    }
  return (struct process_set) {nprocesses, process};
}
int compare(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}
long calculateMedian(struct process_list* list) {
    int count = 0;

    // Count the number of elements in the list
    struct process* current;
    TAILQ_FOREACH(current, list, pointers) {
        count++;
    }

    if (count == 0) {
        // List is empty
        return 0.0;
    }

    // Create an array to store the values
    int* values = (int*)malloc(count * sizeof(int));
    if (values == NULL) {
        perror("Memory allocation failed");
        exit(1);
    }

    // Traverse the list and store values in the array
    current = TAILQ_FIRST(list);
    int i = 0;
    while (current != NULL) {
        values[i] = current->cpu_consumption;
        current = TAILQ_NEXT(current, pointers);
        i++;
    }
    //for(int j = 0; j < i; j++) {
      //  printf("cpu consumption array value: %ld\n", values[j]);
    //}
    // Sort the array (you can use any sorting algorithm)
    qsort(values, count, sizeof(int), compare);

    // Calculate the median
    float median;
    if (count % 2 == 0) {
        // If the count is even, take the average of the two middle values
        median = (values[count / 2 - 1] + values[count / 2]) / 2.0;
    } else {
        // If the count is odd, take the middle value
        median = values[count / 2];
    }

    // Free the allocated memory
    free(values);
    long roundedMedian = round(median);
    return roundedMedian;
}
int
main (int argc, char *argv[])
{
  if (argc != 3)
    {
      fprintf (stderr, "%s: usage: %s file quantum\n", argv[0], argv[0]);
      return 1;
    }

  struct process_set ps = init_processes (argv[1]);
  long quantum_length = (strcmp (argv[2], "median") == 0 ? -1
			 : next_int_from_c_str (argv[2]));
  if (quantum_length == 0)
    {
      fprintf (stderr, "%s: zero quantum length\n", argv[0]);
      return 1;
    }

  struct process_list list;
  TAILQ_INIT (&list);
  long total_wait_time = 0;
  long total_response_time = 0;

  /* Your code here */
  bool isDynamic = false;
  if(quantum_length == -1) {
      isDynamic = true;
      quantum_length = 1;
  }
  for (long i = 0; i < ps.nprocesses; i++) {
      ps.process[i].first_queue = true;
      ps.process[i].remaining_time = ps.process[i].burst_time;
      ps.process[i].cpu_consumption = 0;
      ps.process[i].first_run = false;
  }
    long timer = ps.process[0].arrival_time;
    ps.process[0].first_queue = false;
    TAILQ_INSERT_TAIL(&list, &ps.process[0], pointers);

    while (!TAILQ_EMPTY(&list)) {
        struct process *current_process = TAILQ_FIRST(&list);
        printf("current process pid: %ld\n", current_process->pid);
        printf("timer: %ld\n", timer);
        TAILQ_REMOVE(&list, current_process, pointers); // pop off queue

        if(!current_process->first_run) {
            total_response_time += timer - current_process->arrival_time;
            current_process->first_run = true;
        }
        if (current_process->remaining_time <= quantum_length && current_process->arrival_time <= timer) {
            long current_time = timer;
            while(timer < current_time + current_process->remaining_time) {
                for (long i = 1; i < ps.nprocesses; i++) {
                    if(ps.process[i].first_queue && ps.process[i].arrival_time <= timer) {
                        TAILQ_INSERT_TAIL(&list, &ps.process[i], pointers);
                        ps.process[i].first_queue = false;
                    }
                }
                timer++;
            }
            current_process->completion_time = timer;
            total_wait_time += current_process->completion_time - current_process->arrival_time - current_process->burst_time;
            current_process->cpu_consumption += current_process->remaining_time;
        }
        else if (current_process->arrival_time <= timer) {
            long current_time = timer;
            while(timer < current_time + quantum_length) {
                for (long i = 1; i < ps.nprocesses; i++) {
                    if(ps.process[i].first_queue && ps.process[i].arrival_time <= timer) {
                        TAILQ_INSERT_TAIL(&list, &ps.process[i], pointers);
                        ps.process[i].first_queue = false;
                    }
                }
                timer++;
            }
            current_process->remaining_time = current_process->remaining_time - quantum_length;
            current_process->cpu_consumption += quantum_length;
            TAILQ_INSERT_TAIL(&list, current_process, pointers);
        }
        timer++;
        long median;
        if(isDynamic) {
            median = calculateMedian(&list);
            quantum_length = median;
        }
    }
  /* End of "Your code here" */

  printf ("Average wait time: %.2f\n",
	  total_wait_time / (double) ps.nprocesses);
  printf ("Average response time: %.2f\n",
	  total_response_time / (double) ps.nprocesses);

  if (fflush (stdout) < 0 || ferror (stdout))
    {
      perror ("stdout");
      return 1;
    }

  free (ps.process);
  return 0;
}






