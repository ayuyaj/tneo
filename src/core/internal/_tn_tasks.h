/*******************************************************************************
 *
 * TNeoKernel: real-time kernel initially based on TNKernel
 *
 *    TNKernel:                  copyright © 2004, 2013 Yuri Tiomkin.
 *    PIC32-specific routines:   copyright © 2013, 2014 Anders Montonen.
 *    TNeoKernel:                copyright © 2014       Dmitry Frank.
 *
 *    TNeoKernel was born as a thorough review and re-implementation of
 *    TNKernel. The new kernel has well-formed code, inherited bugs are fixed
 *    as well as new features being added, and it is tested carefully with
 *    unit-tests.
 *
 *    API is changed somewhat, so it's not 100% compatible with TNKernel,
 *    hence the new name: TNeoKernel.
 *
 *    Permission to use, copy, modify, and distribute this software in source
 *    and binary forms and its documentation for any purpose and without fee
 *    is hereby granted, provided that the above copyright notice appear
 *    in all copies and that both that copyright notice and this permission
 *    notice appear in supporting documentation.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE DMITRY FRANK AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL DMITRY FRANK OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *    THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#ifndef __TN_TASKS_H
#define __TN_TASKS_H

/*******************************************************************************
 *    INCLUDED FILES
 ******************************************************************************/

#include "_tn_sys.h"





#ifdef __cplusplus
extern "C"  {     /*}*/
#endif

/*******************************************************************************
 *    PUBLIC TYPES
 ******************************************************************************/

/*******************************************************************************
 *    GLOBAL VARIABLES
 ******************************************************************************/




/*******************************************************************************
 *    DEFINITIONS
 ******************************************************************************/

#define _tn_get_task_by_tsk_queue(que)                                   \
   (que ? container_of(que, struct TN_Task, task_queue) : 0)




/*******************************************************************************
 *    PROTECTED FUNCTION PROTOTYPES
 ******************************************************************************/

/**
 * Callback that is given to `_tn_task_first_wait_complete()`, may perform
 * any needed actions before waking task up, e.g. set some data in the `struct
 * TN_Task` that task is waiting for.
 *
 * @param task
 *    Task that is going to be waken up
 * @param user_data_1
 *    Arbitrary user data given to `_tn_task_first_wait_complete()`
 * @param user_data_2
 *    Arbitrary user data given to `_tn_task_first_wait_complete()`
 */
typedef void (_TN_CBBeforeTaskWaitComplete)(
      struct TN_Task   *task,
      void             *user_data_1,
      void             *user_data_2
      );


static inline BOOL _tn_task_is_runnable(struct TN_Task *task)
{
   return !!(task->task_state & TN_TASK_STATE_RUNNABLE);
}

static inline BOOL _tn_task_is_waiting(struct TN_Task *task)
{
   return !!(task->task_state & TN_TASK_STATE_WAIT);
}

static inline BOOL _tn_task_is_suspended(struct TN_Task *task)
{
   return !!(task->task_state & TN_TASK_STATE_SUSPEND);
}

static inline BOOL _tn_task_is_dormant(struct TN_Task *task)
{
   return !!(task->task_state & TN_TASK_STATE_DORMANT);
}

/**
 * Should be called when task_state is NONE.
 *
 * Set RUNNABLE bit in task_state,
 * put task on the 'ready queue' for its priority,
 *
 * if priority is higher than tn_next_task_to_run's priority,
 * then set tn_next_task_to_run to this task and return TRUE,
 * otherwise return FALSE.
 */
void _tn_task_set_runnable(struct TN_Task *task);


/**
 * Should be called when task_state has just single RUNNABLE bit set.
 *
 * Clear RUNNABLE bit, remove task from 'ready queue', determine and set
 * new tn_next_task_to_run.
 */
void _tn_task_clear_runnable(struct TN_Task *task);

void _tn_task_set_waiting(
      struct TN_Task *task,
      struct TN_ListItem *wait_que,
      enum TN_WaitReason wait_reason,
      TN_Timeout timeout
      );

/**
 * @param wait_rc return code that will be returned to waiting task
 */
void _tn_task_clear_waiting(struct TN_Task *task, enum TN_RCode wait_rc);

void _tn_task_set_suspended(struct TN_Task *task);
void _tn_task_clear_suspended(struct TN_Task *task);

void _tn_task_set_dormant(struct TN_Task* task);

void _tn_task_clear_dormant(struct TN_Task *task);

enum TN_RCode _tn_task_activate(struct TN_Task *task);

/**
 * Should be called when task finishes waiting for anything.
 *
 * @param wait_rc return code that will be returned to waiting task
 */
static inline void _tn_task_wait_complete(struct TN_Task *task, enum TN_RCode wait_rc)
{
   _tn_task_clear_waiting(task, wait_rc);

   //-- if task isn't suspended, make it runnable
   if (!_tn_task_is_suspended(task)){
      _tn_task_set_runnable(task);
   }

}


/**
 * calls _tn_task_clear_runnable() for current task, i.e. tn_curr_run_task
 * Set task state to TN_TASK_STATE_WAIT, set given wait_reason and timeout.
 *
 * If non-NULL wait_que is provided, then add task to it; otherwise reset task's task_queue.
 * If timeout is not TN_WAIT_INFINITE, add task to tn_wait_timeout_list
 */
static inline void _tn_task_curr_to_wait_action(
      struct TN_ListItem *wait_que,
      enum TN_WaitReason wait_reason,
      TN_Timeout timeout
      )
{
   _tn_task_clear_runnable(tn_curr_run_task);
   _tn_task_set_waiting(tn_curr_run_task, wait_que, wait_reason, timeout);
}


/**
 * Change priority of any task (runnable or non-runnable)
 */
void _tn_change_task_priority(struct TN_Task *task, int new_priority);

/**
 * When changing priority of the runnable task, this function 
 * should be called instead of plain assignment.
 *
 * For non-runnable tasks, this function should never be called.
 *
 * Remove current task from ready queue for its current priority,
 * change its priority, add to the end of ready queue of new priority,
 * find next task to run.
 */
void  _tn_change_running_task_priority(struct TN_Task *task, int new_priority);

#if 0
#define _tn_task_set_last_rc(rc)  { tn_curr_run_task = (rc); }

/**
 * If given return code is not `#TN_RC_OK`, save it in the task's structure
 */
void _tn_task_set_last_rc_if_error(enum TN_RCode rc);
#endif

#if TN_USE_MUTEXES
/**
 * Check if mutex is locked by task.
 *
 * @return TRUE if mutex is locked, FALSE otherwise.
 */
BOOL _tn_is_mutex_locked_by_task(struct TN_Task *task, struct TN_Mutex *mutex);
#endif

/**
 * Wakes up first (if any) task from the queue, calling provided
 * callback before.
 *
 * @param wait_queue
 *    Wait queue to get first task from
 * @param wait_rc
 *    Code that will be returned to woken-up task as a result of waiting
 *    (this code is just given to `_tn_task_wait_complete()` actually)
 * @param callback
 *    Callback function to call before wake task up, see 
 *    `#_TN_CBBeforeTaskWaitComplete`. Can be `NULL`.
 * @param user_data_1
 *    Arbitrary data that is passed to the callback
 * @param user_data_2
 *    Arbitrary data that is passed to the callback
 *
 *
 * @return
 *    - `TRUE` if queue is not empty and task has woken up
 *    - `FALSE` if queue is empty, so, no task to wake up
 */
BOOL _tn_task_first_wait_complete(
      struct TN_ListItem           *wait_queue,
      enum TN_RCode                 wait_rc,
      _TN_CBBeforeTaskWaitComplete *callback,
      void                         *user_data_1,
      void                         *user_data_2
      );





#ifdef __cplusplus
}  /* extern "C" */
#endif


#endif // __TN_TASKS_H


/*******************************************************************************
 *    end of file
 ******************************************************************************/


