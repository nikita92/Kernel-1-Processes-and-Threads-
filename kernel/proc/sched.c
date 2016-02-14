/******************************************************************************/
/* Important Fall 2014 CSCI 402 usage information:                            */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/******************************************************************************/

#include "globals.h"
#include "errno.h"

#include "main/interrupt.h"

#include "proc/sched.h"
#include "proc/kthread.h"

#include "util/init.h"
#include "util/debug.h"

static ktqueue_t kt_runq;

static __attribute__((unused)) void
sched_init(void)
{
        sched_queue_init(&kt_runq);
}
init_func(sched_init);



/*** PRIVATE KTQUEUE MANIPULATION FUNCTIONS ***/
/**
 * Enqueues a thread onto a queue.
 *
 * @param q the queue to enqueue the thread onto
 * @param thr the thread to enqueue onto the queue
 */
static void
ktqueue_enqueue(ktqueue_t *q, kthread_t *thr)
{
        KASSERT(!thr->kt_wchan);
        list_insert_head(&q->tq_list, &thr->kt_qlink);
        thr->kt_wchan = q;
        q->tq_size++;
}

/**
 * Dequeues a thread from the queue.
 *
 * @param q the queue to dequeue a thread from
 * @return the thread dequeued from the queue
 */
	static kthread_t *
ktqueue_dequeue(ktqueue_t *q)
{
        kthread_t *thr;
        list_link_t *link;

        if (list_empty(&q->tq_list))
                return NULL;

        link = q->tq_list.l_prev;
        thr = list_item(link, kthread_t, kt_qlink);
        list_remove(link);
        thr->kt_wchan = NULL;

        q->tq_size--;

        return thr;
}

/**
 * Removes a given thread from a queue.
 *
 * @param q the queue to remove the thread from
 * @param thr the thread to remove from the queue
 */
static void
ktqueue_remove(ktqueue_t *q, kthread_t *thr)
{
        KASSERT(thr->kt_qlink.l_next && thr->kt_qlink.l_prev);
        list_remove(&thr->kt_qlink);
        thr->kt_wchan = NULL;
        q->tq_size--;
}

/*** PUBLIC KTQUEUE MANIPULATION FUNCTIONS ***/
void
sched_queue_init(ktqueue_t *q)
{
        list_init(&q->tq_list);
        q->tq_size = 0;
}

int
sched_queue_empty(ktqueue_t *q)
{
        return list_empty(&q->tq_list);
}

/*
 * Updates the thread's state and enqueues it on the given
 * queue. Returns when the thread has been woken up with wakeup_on or
 * broadcast_on.
 *
 * Use the private queue manipulation functions above.
 */
void
sched_sleep_on(ktqueue_t *q)
{
	/* Here I am assuming that q is the queue on which I want the current thread to sleep on */
	/* 1 .I simply have to set the p_thread state as Sleepable with KT_SLEEP */
	/* 2 .The current thread has to be sent into sleep */
		curthr->kt_state=KT_SLEEP;
	/* 3 .Then enqueue into the q using ktqueue_enqueue(ktqueue_t *q, kthread_t *thr)*/
		ktqueue_enqueue(q,curthr);
	/* 4. change the state of the thread currently executing */
		sched_switch();
	/* return when the thread has been woken up with wakeup_on */
}


/*
 * Similar to sleep on, but the sleep can be cancelled.
 *
 * Don't forget to check the kt_cancelled flag at the correct times.
 *
 * Use the private queue manipulation functions above.
 */
int
sched_cancellable_sleep_on(ktqueue_t *q)
{
	/* Here I am assuming that q is the queue on which I want the current thread to sleep on */
		/* 1 .I simply have to set the p_thread state as Sleepable with KT_SLEEP_CANCELLABLE */
		/* 2 .The current thread has to be sent into sleep */
			curthr->kt_state=KT_SLEEP_CANCELLABLE;
		/* 3 .Then enqueue into the q using ktqueue_enqueue(ktqueue_t *q, kthread_t *thr)*/
			if(!curthr->kt_cancelled){
				ktqueue_enqueue(q,curthr);
				/* 4. change the state of the thread currently executing */
				sched_switch();
				
				if(curthr->kt_cancelled)
					return -EINTR;
				else	
					return 0;
			}else{
				/* return when the thread has been woken up with wakeup_on */
				/* I am not sure what to do if the current thread
				 * is supposed to be cancelled even before being sent
				 * to the sleep. Returnig -1 for now
				 */
				return -EINTR;
			}


}

kthread_t *
sched_wakeup_on(ktqueue_t *q)
{
	kthread_t *thread=NULL;
	if(!sched_queue_empty(q)){
		/*1. Needs to dequeue the first thread in the q*/
		thread = ktqueue_dequeue(q);
		KASSERT((thread->kt_state==KT_SLEEP)||(thread->kt_state==KT_SLEEP_CANCELLABLE));
		dbg(DBG_PRINT, "(GRADING#1A 4.a): The thread to be woken up, currently has state as Sleep or Sleep Cancellable\n");
		/*2. Set the p_state as KT_RUN */
		/*thread->kt_state=KT_RUN;*/
		sched_make_runnable(thread);


		/*3. return thread */
		return thread;
	}else{
		return NULL;
	}
}

/*
 * Not sure about this .need to discuss
 */
void
sched_broadcast_on(ktqueue_t *q)
{
	/*I think we will have to call the sched_wakeup_on function in a loop*/
	while(!sched_queue_empty(q)){
		sched_wakeup_on(q);
	}
}

/*
 * If the thread's sleep is cancellable, we set the kt_cancelled
 * flag and remove it from the queue. Otherwise, we just set the
 * kt_cancelled flag and leave the thread on the queue.
 *
 * Remember, unless the thread is in the KT_NO_STATE or KT_EXITED
 * state, it should be on some queue. Otherwise, it will never be run
 * again.
 */
void
sched_cancel(struct kthread *kthr)
{
	KASSERT(kthr->kt_state != KT_NO_STATE);

	kthr->kt_cancelled = 1;

	if(kthr->kt_state == KT_SLEEP_CANCELLABLE)
	{
		ktqueue_remove(kthr->kt_wchan, kthr);
		sched_make_runnable(kthr);
	}
	/* I think we have dequeue each thread from
	   We will have to check if the thread is allowed to be canceled
	   Actually each thread has the current location in which the thread is blocked in the wtchan .
	   But I think we will have to check it only if the previous condition is correct.
	   then we have to call the remove_k_thread*/
	/* I dont understand if we have to free the contents of the thread before removing it
        NOT_YET_IMPLEMENTED("PROCS: sched_cancel");*/
}

/*
 * In this function, you will be modifying the run queue, which can
 * also be modified from an interrupt context. In order for thread
 * contexts and interrupt contexts to play nicely, you need to mask
 * all interrupts before reading or modifying the run queue and
 * re-enable interrupts when you are done. This is analagous to
 * locking a mutex before modifying a data structure shared between
 * threads. Masking interrupts is accomplished by setting the IPL to
 * high.
 *
 * Once you have masked interrupts, you need to remove a thread from
 * the run queue and switch into its context from the currently
 * executing context.
 *
 * If there are no threads on the run queue (assuming you do not have
 * any bugs), then all kernel threads are waiting for an interrupt
 * (for example, when reading from a block device, a kernel thread
 * will wait while the block device seeks). You will need to re-enable
 * interrupts and wait for one to occur in the hopes that a thread
 * gets put on the run queue from the interrupt context.
 *
 * The proper way to do this is with the intr_wait call. See
 * interrupt.h for more details on intr_wait.
 *
 * Note: When waiting for an interrupt, don't forget to modify the
 * IPL. If the IPL of the currently executing thread masks the
 * interrupt you are waiting for, the interrupt will never happen, and
 * your run queue will remain empty. This is very subtle, but
 * _EXTREMELY_ important.
 *
 * Note: Don't forget to set curproc and curthr. When sched_switch
 * returns, a different thread should be executing than the thread
 * which was executing when sched_switch was called.
 *
 * Note: The IPL is process specific.
 */
void
sched_switch(void)
{
	kthread_t *thread = NULL;
	kthread_t *old_thread = NULL;
	uint8_t old_ipl = IPL_LOW;
	/*1. disable the interrupt */
	intr_disable();
	old_ipl = intr_getipl();
	intr_setipl(IPL_HIGH);
	/*2. Get the current context for the current thread */

	/*5. Check if the queue is empty*/
	while(sched_queue_empty(&kt_runq)){
		/*6. enable the thread */
		intr_setipl(IPL_LOW);
		intr_enable();
		/*7. call the intr_wait to wait on the interrupt since there are no threads on the queue*/
		intr_wait();
		/*8. Try to get the thread again from the runq */
		intr_disable();
		intr_setipl(IPL_HIGH);
	}
	/*3. remove a thread from the run queue*/
	thread = ktqueue_dequeue(&kt_runq);
	old_thread = curthr;

	/*8. it's important that we update the current proc and thread before entering the new context*/
	curproc = thread->kt_proc;
	curthr = thread;
	
	/*4. switch into its context from the currently executing context
		  	     ; using  */
	context_switch(&old_thread->kt_ctx,&thread->kt_ctx);
	
	intr_setipl(old_ipl);
	intr_enable();
}

/*
 * Since we are modifying the run queue, we _MUST_ set the IPL to high
 * so that no interrupts happen at an inopportune moment.

 * Remember to restore the original IPL before you return from this
 * function. Otherwise, we will not get any interrupts after returning
 * from this function.
 *
 * Using intr_disable/intr_enable would be equally as effective as
 * modifying the IPL in this case. However, in some cases, we may want
 * more fine grained control, making modifying the IPL more
 * suitable. We modify the IPL here for consistency.
 */
void
sched_make_runnable(kthread_t *thr)
{
         KASSERT(&kt_runq!=thr->kt_wchan);
         dbg(DBG_PRINT, "(GRADING#1A 4.b): The thread is not already on the runq\n");
	/*1. get the current interupt. Need to check how to get this.*/
	/*2. then we will have to setup the IPL to HIGH*/
	intr_disable();
	/*3. set the thr to KT_RUN*/
	thr->kt_state = KT_RUN;
	/*4. We have to enqueue the current thread in the Run queue*/
	ktqueue_enqueue(&kt_runq,thr);
	/*5. We will have to set the IPL to previous IPL*/
	intr_enable();
}
