This directory contains experimental APIs for farming tasks out to a pool of worker threads. Everything in this directory is still highly in flux, and is not anywhere near ready to ship.

thread_pool.{h,cc} contain a class that manages a pool of worker threads and can distribute work to them.
thread_pool_thread.{h,cc} is a subclass of WorkerThread that these distirubted tasks run on.

worker_task_queue.{h,cc,idl} exposes two APIs:
* postFunction - a simple API for posting a task to a worker.
* postTask - an API for posting tasks that can specify other tasks as prerequisites and coordinates the transfer of return values of prerequisite tasks to dependent tasks

task_worklet.{h,cc,idl}, task_worklet_global_scope.{h,cc,idl}, and window_task_worklet.{h,idl} exposes a similar API to
WorkerTaskQueue, built on top of the Worklet API. In addition to implementing postTask() just like WorkerTaskQueue, it
includes addModule() (inherited from Worklet) and a second variant of postTask(). Modules loaded via addModule() can call
registerTask() to specify a name and class with a process() function, and this second variant of postTask() takes a task name
instead of a function. If a task is registered with that given task name, its process() function will be called with the given
parameters.

task.{h,cc,idl} exposes the simple wrapper object returned by postTask and provides the backend that runs tasks on the worker thread (for both postFunction and postTask) and tracks the relationships between tasks (for postTask).
