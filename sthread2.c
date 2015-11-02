#ifndef STHREAD_C
#define STHREAD_C
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
struct sigaction act;
#define MAX_THREAD 20
#define MEM 64000
int id,count;
ucontext_t Main;
struct itimerval it;
ucontext_t last;
struct sthread_t{
	int s_id;
	int s_size;
	ucontext_t * thread;
};
struct thread_status{
	int s_id;
	struct thread_status * next;
};
struct queue{
	struct sthread_t * task;
	struct queue * next;
};
struct queue * head,*tail;
struct sthread_t * currrent_thread;
struct thread_status * running,*waiting;
void sthread_wait();
void kill_thread();
void init(struct itimerval it);
void schedule(int l);
void add_to_running(int t_id);
int check_waiting(int t,struct thread_status * );
int sthread_self();



int sthread_self()
{
	return currrent_thread->s_id;
}


void sthread_init(struct sthread_t  * new_task){
		new_task->thread=(ucontext_t *)malloc(sizeof(ucontext_t));
		getcontext(new_task->thread);
		getcontext(&last);
		last.uc_stack.ss_sp=malloc(MEM);
		last.uc_stack.ss_size=MEM;
		last.uc_stack.ss_flags=0;
		makecontext(&last,kill_thread,0);
		new_task->thread->uc_link=&last;
		new_task->thread->uc_stack.ss_sp=malloc(MEM);
		new_task->thread->uc_stack.ss_size=MEM;
		new_task->thread->uc_stack.ss_flags=0;
		new_task->s_size=MEM;
}
void block()
{
	sigfillset(&act);
	sigprocmask(SIG_BLOCK,&act,NULL); //block all signals
}
void unblock()
{
	sigprocmask(SIG_UNBLOCK,&act,NULL);   //restore blocked signals
	sigemptyset(&act);
}
int add_to_queue(struct sthread_t * new_task,int is_new)
{
	printf("adding to queue\n");
	block();
		if(head==NULL)  //this is the first thread
		{
			init(it);
			struct sthread_t * Main_task=(struct sthread_t *)malloc(sizeof(struct sthread_t) );
			sthread_init(Main_task);
			Main_task->s_id=-1;
			printf("done timer part\n");
			head=(struct queue *)malloc(sizeof(struct queue));
			tail=(struct queue *)malloc(sizeof(struct queue));
			tail->next=NULL;
			head->next=tail;
			head->task=Main_task;
			tail->task=new_task;
			new_task->s_id=id;
			add_to_running(new_task->s_id);
			currrent_thread=head->task;
			id++;
			count++;
			unblock();
			return 1;
		}
		else
		{
			if(count==MAX_THREAD)
			{
				unblock();
				return -1;
			}
			else
			{
				struct queue * temp=(struct queue *)malloc(sizeof(struct queue));
				temp->next=NULL;
				temp->task=new_task;
				if(is_new){
					new_task->s_id=id;
					id++;
					count++;
					add_to_running(new_task->s_id);
				}
				tail->next=temp;
				tail=temp;
				if(is_new==1)
				unblock();
				return 0;
			}
		}
}
void kill_thread()
{
	block();
	ucontext_t temp;
	if(head==NULL)
	{
		currrent_thread=NULL;
		it.it_interval.tv_sec=0;
		it.it_interval.tv_usec=0;
		it.it_value.tv_sec=0;
		it.it_value.tv_usec=0;
		unblock();
		fflush(stdout);
		setitimer(ITIMER_PROF,&it,NULL);
		return ;
	}
	else
	{
		currrent_thread=head->next->task;
		delete_running(head->task->s_id,running);
		struct queue * temp=head;
		head=head->next;
		free(temp);
		count--;
		unblock();
		fflush(stdout);     //flushing out all standard output
		swapcontext(&temp,currrent_thread->thread);
	}
}
int sthread_create(struct sthread_t * new,void (* fn)(),void * arg)
{
	printf("Receive rquest\n");
	sthread_init(new);
	printf("init done\n");
	makecontext(new->thread,fn,1,arg);
	printf("new context done");
	int ret=add_to_queue(new,1);
	return ret;
}
void schedule(int l)
{
	block();
	printf("scehduling thread\n");
	  if(currrent_thread==NULL)    //no thread executing till now apart from main thread
	{
		currrent_thread=head->task;
		printf("swapping \n");
		unblock();
		swapcontext(&Main,currrent_thread->thread);
	}
	else
	{
		if(head->next==NULL) //this is the last thread
		{
			it.it_interval.tv_sec=0;
	 		it.it_interval.tv_usec=0;
	 		it.it_value.tv_sec=0;
	 		it.it_value.tv_usec =0;
	 		struct queue * temp=head;
	 		head=head->next;
	 		unblock();
	 		setitimer(ITIMER_PROF, &it, NULL);
			setcontext(temp->task->thread);
		}
		else
		{
			currrent_thread=head->next->task;
			struct sthread_t * t=head->task;
			struct query * temp=head;
			add_to_queue(t,0);
			head=head->next;
			free(temp);
			unblock();
			swapcontext(t->thread,currrent_thread->thread);
		}
	}
}
void init(struct itimerval it)
{
	 struct sigaction act, oact;
	 act.sa_handler=schedule;
	 sigemptyset(&act.sa_mask);
	 act.sa_flags = 0;
	 sigaction(SIGPROF, &act, &oact);
	 it.it_interval.tv_sec=0;
	 it.it_interval.tv_usec=1000;
	 it.it_value.tv_sec=0;
	 it.it_value.tv_usec =5000;
	 setitimer(ITIMER_PROF, &it, NULL);
}
void sthread_wait(struct sthread_t * thread){
	while(check_waiting(thread->s_id,running)){
			schedule(0);
	}
}
void add_to_running(int t_id)
{
		struct thread_status * temp=(struct thread_status *)malloc(sizeof(struct thread_status));
		temp->next=running;
		temp->s_id=t_id;
		running=temp;
		return ;
}
void delete_running(int t_id,struct thread_status * head){
	struct thread_status * temp=head;
	if(temp->s_id==t_id)
	{
		running=temp->next;
		free(temp);
	}
	else{
		while(temp->next!=NULL){
			if(temp->next->s_id==t_id)
			{
				struct thread_status * temp2=temp->next;
				temp->next=temp->next->next;
				free(temp2);
				break;
			}
			temp=temp->next;
		}
	}
}
int check_waiting(int t_id,struct thread_status * head){
	struct thread_status * temp;
	temp=head;
	while(temp!=NULL)
	{
		if(temp->s_id==t_id)
			return 1;
			temp=temp->next;
	}
	return 0;
}
#endif
