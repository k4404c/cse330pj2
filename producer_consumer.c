#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/tty.h>
#include <linux/uidgid.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/param.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/time_namespace.h>
#include <linux/time.h>

#include <linux/proc_fs.h>
#include <linux/slab.h>

#define MAX_BUFFER_SIZE 500
#define MAX_NO_OF_PRODUCERS 1
#define MAX_NO_OF_CONSUMERS 100

#define PCINFO(s, ...) pr_info("###[%s]###" s, __FUNCTION__, ##__VA_ARGS__)

unsigned long long total_time_elapsed = 0;

// use this struct to store the process information
struct process_info
{
	unsigned long pid;
	unsigned long long start_time;
	unsigned long long boot_time;
} process_default_info = {0, 0};

int total_no_of_process_produced = 0;
int total_no_of_process_consumed = 0;

int end_flag = 0;

char producers[MAX_NO_OF_PRODUCERS][12] = {"kProducer-X"};
char consumers[MAX_NO_OF_CONSUMERS][12] = {"kConsumer-X"};

static struct task_struct *ctx_producer_thread[MAX_NO_OF_PRODUCERS];
static struct task_struct *ctx_consumer_thread[MAX_NO_OF_CONSUMERS];

// use fill and use to keep track of the buffer
struct process_info buffer[MAX_BUFFER_SIZE];
int fill = 0;
int use = 0;

// COMPLETED TODO Define your input parameters (buffSize, prod, cons, uuid) here
// Then use module_param to pass them from insmod command line. (--Assignment 2)
static int buffSize = MAX_BUFFER_SIZE;
module_param(buffSize, int, 0);
MODULE_PARM_DESC(buffSize, "Size of the buffer");

static int prod = MAX_NO_OF_PRODUCERS;
module_param(prod, int, 0);
MODULE_PARM_DESC(prod, "Number of producers");

static int cons = MAX_NO_OF_CONSUMERS;
module_param(cons, int, 0);
MODULE_PARM_DESC(cons, "Number of consumers");

static int uuid = 0;
module_param(uuid, int, 0);
MODULE_PARM_DESC(uuid, "UUID");
//--------------------------------------------------------------//


struct semaphore empty, full, mutex;

static int __init init_thread(void)
{
    // COMPELTED TODO Initialize semaphores here -- Assignment 3
    sema_init(&empty, buffSize);
    sema_init(&full, 0);
    sema_init(&mutex, 1);

	return 0;
}

int producer_thread_function(void *pv)
{
	allow_signal(SIGKILL);
	struct task_struct *task;

	for_each_process(task)
	{
		if (task->cred->uid.val == uiud)//simple_strtoul(uuid, NULL, 10)) // cahnged, was uuid
		{
			// COMPLETED TODO Implement your producer kernel thread here
			// use kthread_should_stop() to check if the kernel thread should stop
			// use down() and up() for semaphores
			// Hint: Please refer to sample code to see how to use process_info struct
			// Hint: kthread_should_stop() should be checked after down() and before up()

			while (1)
            {
                down(&empty);
                down(&mutex);

				 if (kthread_should_stop())
				 {
                    break;
				 }
                // Produce item into the buffer
                buffer[fill] = process_default_info;
                buffer[fill].pid = task->pid;
                buffer[fill].start_time = ktime_get_ns();
                buffer[fill].boot_time = current_kernel_time().tv_nsec;

                fill = (fill + 1) % buffSize;

                up(&mutex);
                up(&full);

                total_no_of_process_produced++;
                PCINFO("[%s] Produce-Item#:%d at buffer index: %d for PID:%d \n", current->comm,
                       total_no_of_process_produced, (fill + buffSize - 1) % buffSize, task->pid);

               

                schedule_timeout_interruptible(msecs_to_jiffies(1000)); // sleep for 1 second
            }
        }
	}

	PCINFO("[%s] Producer Thread stopped.\n", current->comm);
	ctx_producer_thread[0] = NULL;
	return 0;
}

int consumer_thread_function(void *pv)
{
	allow_signal(SIGKILL);
	int no_of_process_consumed = 0;

	while (!kthread_should_stop()&& !end_flag)
	{
		// COMPELTED TODO Implement your consumer kernel thread here
		// use end_flag (see kernel module exit function) to check if the kernel thread should stop
		// if end_flag, then break
		// use down() and up() for semaphores
		// Hint: Please refer to sample code to see how to use process_info struct
		// Hint: end_flag should be checked after down() and before up()
        down(&full);
        down(&mutex);

		struct process_info consumed_process = buffer[use];
        use = (use + 1) % buffSize;

        up(&mutex);
        up(&empty);

		unsigned long long ktime = ktime_get_ns();
		unsigned long long process_time_elapsed = (ktime - consumed_process.start_time) / 1000000000;
		total_time_elapsed += ktime - consumed_process.start_time;

		unsigned long long process_time_hr = process_time_elapsed / 3600;
		unsigned long long process_time_min = (process_time_elapsed - 3600 * process_time_hr) / 60;
		unsigned long long process_time_sec = (process_time_elapsed - 3600 * process_time_hr) - (process_time_min * 60);

		no_of_process_consumed++;
		total_no_of_process_consumed++;
		//PCINFO("[%s] Consumed Item#-%d on buffer index:%d::PID:%lu \t Elapsed Time %llu:%llu:%llu \n", current->comm,
			   //no_of_process_consumed, (use + buffSize - 1) % buffSize, process_pid, process_time_hr, process_time_min, process_time_sec);
		//schedule_timeout_interruptible(msecs_to_jiffies(1000)); // sleep for 1 second
	}
	PCINFO("[%s] Consumed Item#-%d on buffer index:%d::PID:%lu \t Elapsed Time %llu:%llu:%llu \n", current->comm,
			   no_of_process_consumed, (use + buffSize - 1) % buffSize, consumed_process.pid, process_time_hr, process_time_min, process_time_sec);
	PCINFO("[%s] Consumer Thread stopped.\n", current->comm);
	return 0;
	
}

char *replace_char(char *str, char find, char replace)
{
	char *current_pos = strchr(str, find);
	while (current_pos)
	{
		*current_pos = replace;
		current_pos = strchr(current_pos, find);
	}
	return str;
}

void name_threads(void)
{
	for (int index = 0; index < prod; index++)
	{
		char id = (index + 1) + '0';
		strcpy(producers[index], "kProducer-X");
		strcpy(producers[index], replace_char(producers[index], 'X', id));
	}

	for (int index = 0; index < cons; index++)
	{
		char id = (index + 1) + '0';
		strcpy(consumers[index], "kConsumer-X");
		strcpy(consumers[index], replace_char(consumers[index], 'X', id));
	}
}

static int __init thread_init_module(void)
{
	PCINFO("CSE330 Project-2 Kernel Module Inserted\n");
	PCINFO("Kernel module received the following inputs: UID:%d, Buffer-Size:%d, No of Producer:%d, No of Consumer:%d", , buffSize, prod, cons);

	if (buffSize > 0 && (prod >= 0 && prod < 2))
	{
		// COMPLETED TODO initialize the semaphores here

		name_threads();
		sema_init(&empty, buffSize);
		sema_init(&full, 0);
		sema_init(&mutex, 1);

		name_threads();
		for (int index = 0; index < buffSize; index++)
			buffer[index] = process_default_info;

		// COMPLETED TODO use kthread_run to create producer kernel threads here
		// Hint: Please refer to sample code to see how to use kthread_run, kthread_should_stop, kthread_stop, etc.
		// Hint: use ctx_producer_thread[index] to store the return value of kthread_run
		for (int index = 0; index < prod; index++)
		{
    ctx_producer_thread[index] = kthread_run(producer_thread_function, NULL, "kProducer-%d", index + 1);
    		if (IS_ERR(ctx_producer_thread[index]))
    		{
        		PCINFO("Failed to create producer thread %d\n", index + 1);
        		return -ENOMEM;
    		}
		}
		// COMPLETED TODO use kthread_run to create consumer kernel threads here
		// Hint: Please refer to sample code to see how to use kthread_run, kthread_should_stop, kthread_stop, etc.
		// Hint: use ctx_consumer_thread[index] to store the return value of kthread_run
		for (int index = 0; index < cons; index++)
		{
    		ctx_consumer_thread[index] = kthread_run(consumer_thread_function, NULL, "kConsumer-%d", index + 1);
    		if (IS_ERR(ctx_consumer_thread[index]))
   			{
        		PCINFO("Failed to create consumer thread %d\n", index + 1);
        		return -ENOMEM;
    		}
		}
	}
	else
	{
		// Input Validation Failed
		PCINFO("Incorrect Input Parameter Configuration Received. No kernel threads started. Please check input parameters.");
		PCINFO("The kernel module expects buffer size (a positive number) and # of producers(0 or 1) and # of consumers > 0");
	}

	return 0;
}

static void __exit thread_exit_module(void)
{
	if (buffSize > 0)
	{
		while (1)
		{
			if (total_no_of_process_consumed == total_no_of_process_produced || !cons || !prod)
			{
				if (!cons)
				{
					up(&empty);
				}

				for (int index = 0; index < prod; index++)
				{
					if (ctx_producer_thread[index])
					{
						kthread_stop(ctx_producer_thread[index]);
					}
				}

				end_flag = 1;

				for (int index = 0; index < cons; index++)
				{
					up(&full);
					up(&mutex);
				}

				for (int index = 0; index < cons; index++)
				{
					if (ctx_consumer_thread[index]){
						kthread_stop(ctx_consumer_thread[index]);
					}
				}
				break;
			}
			else
				continue;
		}

		// total_time_elapsed is now in nsec
		total_time_elapsed = total_time_elapsed / 1000000000;

		unsigned long long total_time_hr = total_time_elapsed / 3600;
		unsigned long long total_time_min = (total_time_elapsed - 3600 * total_time_hr) / 60;
		unsigned long long total_time_sec = (total_time_elapsed - 3600 * total_time_hr) - (total_time_min * 60);

		PCINFO("Total number of items produced: %d", total_no_of_process_produced);
		PCINFO("Total number of items consumed: %d", total_no_of_process_consumed);
		PCINFO("The total elapsed time of all processes for UID %d is \t%llu:%llu:%llu  \n", , total_time_hr, total_time_min, total_time_sec);
	}

	PCINFO("CSE330 Project 2 Kernel Module Removed\n");
}

module_init(thread_init_module);
module_exit(thread_exit_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kortney Curry, Eric Rodriguez");
MODULE_DESCRIPTION("CSE330 2024 Spring Project 2 Process Management\n");
MODULE_VERSION("0.1");
