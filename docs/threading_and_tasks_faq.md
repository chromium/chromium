# Threading and Tasks in Chrome - FAQ

[TOC]

Note: Make sure to read the main [Threading and Tasks](threading_and_tasks.md)
docs first.

## General

### On which thread will a task run?

A task is posted through the `base/task/thread_pool.h` API with `TaskTraits`.

* If `TaskTraits` contain `BrowserThread::UI`:
    * The task runs on the main thread.

* If `TaskTraits` contain `BrowserThread::IO`:
    * The task runs on the IO thread.

* If `TaskTraits` don't contain `BrowserThread::UI/IO`:
    * If the task is posted through a `SingleThreadTaskRunner` obtained from
      `CreateSingleThreadTaskRunner(..., mode)`:
        * Where `mode` is `SingleThreadTaskRunnerThreadMode::DEDICATED`:
            * The task runs on a thread that only runs tasks from that
              SingleThreadTaskRunner. This is not the main thread nor the IO
              thread.

        * Where `mode` is `SingleThreadTaskRunnerThreadMode::SHARED`:
            * The task runs on a thread that runs tasks from one or many
              unrelated SingleThreadTaskRunners. This is not the main thread nor
              the IO thread.

    * Otherwise:
        * The task runs in a thread pool.

As explained in [Prefer Sequences to Threads](threading_and_tasks.md#Prefer-Sequences-to-Threads),
tasks should generally run on a sequence in a thread pool rather than on a
dedicated thread.

### Does release of a TaskRunner block on posted tasks?

Releasing a TaskRunner reference does not wait for tasks previously posted to
the TaskRunner to complete their execution. Tasks can run normally after the
last client reference to the TaskRunner to which they were posted has been
released and it can even be kept alive indefinitely through
`SequencedTaskRunner::GetCurrentDefault()` or
`SingleThreadTaskRunner::GetCurrentDefault()`.

If you want some state to be deleted only after all tasks currently posted to a
SequencedTaskRunner have run, store that state in a helper object and schedule
deletion of that helper object on the SequencedTaskRunner using
`base::OnTaskRunnerDeleter` after posting the last task. See
[example CL](https://crrev.com/c/1416271/15/chrome/browser/performance_monitor/system_monitor.h).
But be aware that any task posting back to its "current" sequence can enqueue
itself after that "last" task.

## Making blocking calls (which do not use the CPU)

### How to make a blocking call without preventing other tasks from being scheduled?

The steps depend on where the task runs (see [Where will a task run?](#On-what-thread-will-a-task-run_)).

If the task runs in a thread pool:

* Annotate the scope that may block with
  `ScopedBlockingCall(BlockingType::MAY_BLOCK/WILL_BLOCK)`. A few milliseconds
  after the annotated scope is entered, the capacity of the thread pool is
  incremented. This ensures that your task doesn't reduce the number of tasks
  that can run concurrently on the CPU. If the scope exits, the thread pool
  capacity goes back to normal.

If the task runs on the main thread, the IO thread or a `SHARED
SingleThreadTaskRunner`:

* Blocking on one of these threads will cause breakages. Move your task to a
  thread pool (or to a `DEDICATED SingleThreadTaskRunner` if necessary - see
  [Prefer Sequences to Threads](threading_and_tasks.md#Prefer-Sequences-to-Threads)).

If the task runs on a `DEDICATED SingleThreadTaskRunner`:

* Annotate the scope that may block with
  `ScopedBlockingCall(BlockingType::MAY_BLOCK/WILL_BLOCK)`. The annotation is a
  no-op that documents the blocking behavior (and makes it pass assertions).
  Tasks posted to the same `DEDICATED SingleThreadTaskRunner` won't run until
  your blocking task returns (they will never run if the blocking task never
  returns).

[base/threading/scoped_blocking_call.h](https://cs.chromium.org/chromium/src/base/threading/scoped_blocking_call.h)
explains the difference between `MAY_BLOCK` and `WILL_BLOCK` and gives
examples of blocking operations.

### How to make a blocking call that may never return without preventing other tasks from being scheduled?

If you can't avoid making a call to a third-party library that may block off-
CPU, follow recommendations in [How to make a blocking call without affecting
other tasks?](#How-to-make-a-blocking-call-without-affecting-other-tasks_).
This ensures that a current task doesn't prevent other tasks from running even
if it never returns.

Since tasks posted to the same sequence can't run concurrently, it is advisable
to run tasks that may block indefinitely in
[parallel](threading_and_tasks.md#posting-a-parallel-task) rather than in
[sequence](threading_and_tasks.md#posting-a-sequenced-task) (unless posting many
such tasks at which point sequencing can be a useful tool to prevent flooding).

### Do calls to blocking //base APIs need to be annotated with ScopedBlockingCall?

No. All blocking //base APIs (e.g. `base::ReadFileToString`, `base::File::Read`,
`base::SysInfo::AmountOfFreeDiskSpace`, `base::WaitableEvent::Wait`, etc.) have
their own internal annotations. See
[base/threading/scoped_blocking_call.h](https://cs.chromium.org/chromium/src/base/threading/scoped_blocking_call.h).

### Can multiple ScopedBlockingCall be nested for the purpose of documentation?

Nested `ScopedBlockingCall` are supported. Most of the time, the inner
ScopedBlockingCalls will no-op (the exception is `WILL_BLOCK` nested in `MAY_BLOCK`).
As such, it is permitted to add a ScopedBlockingCall in the scope where a function
that is already annotated is called for documentation purposes.:

```cpp
Data GetDataFromNetwork() {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  // Fetch data from network.
  ...
  return data;
}

void ProcessDataFromNetwork() {
  Data data;
  {
    // Document the blocking behavior with a ScopedBlockingCall.
    // Permitted, but not required since GetDataFromNetwork() is itself annotated.
    ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
    data = GetDataFromNetwork();
  }
  CPUIntensiveProcessing(data);
}
```

 However, CPU usage should always be minimal within the scope of
`ScopedBlockingCall`. See
[base/threading/scoped_blocking_call.h](https://cs.chromium.org/chromium/src/base/threading/scoped_blocking_call.h).


## Sequences

### How to migrate from SingleThreadTaskRunner to SequencedTaskRunner?

The following mappings can be useful when migrating code from a
`SingleThreadTaskRunner` to a `SequencedTaskRunner`:

* base::SingleThreadTaskRunner -> base::SequencedTaskRunner
    * SingleThreadTaskRunner::BelongsToCurrentThread() -> SequencedTaskRunner::RunsTasksInCurrentSequence()
* base::SingleThreadTaskRunner::CurrentDefaultHandle ->
  base::SequencedTaskRunnerHandle::CurrentDefaultHandle
* THREAD_CHECKER -> SEQUENCE_CHECKER
* base::ThreadLocalStorage::Slot -> base::SequenceLocalStorageSlot
* BrowserThread::DeleteOnThread -> base::OnTaskRunnerDeleter / base::RefCountedDeleteOnSequence
* BrowserMessageFilter::OverrideThreadForMessage() -> BrowserMessageFilter::OverrideTaskRunnerForMessage()
* CreateSingleThreadTaskRunner() -> CreateSequencedTaskRunner()
     * Every CreateSingleThreadTaskRunner() usage, outside of
       BrowserThread::UI/IO, should be accompanied with a comment and ideally a
       bug to make it sequence when the sequence-unfriendly dependency is
       addressed.

### How to ensure mutual exclusion between tasks posted by a component?

Create a `SequencedTaskRunner` using `CreateSequencedTaskRunner()` and
store it on an object that can be accessed from all the PostTask() call sites
that require mutual exclusion. If there isn't a shared object that can own a
common `SequencedTaskRunner`, use
`Lazy(Sequenced|SingleThread|COMSTA)TaskRunner` in an anonymous namespace.

## Tests

### How to test code that posts tasks?

If the test uses `BrowserThread::UI/IO`, instantiate a
`content::BrowserTaskEnvironment` for the scope of the test. Call
`BrowserTaskEnvironment::RunUntilIdle()` to wait until all tasks have run.

If the test doesn't use `BrowserThread::UI/IO`, instantiate a
`base::test::TaskEnvironment` for the scope of the test. Call
`base::test::TaskEnvironment::RunUntilIdle()` to wait until all tasks have
run.

In both cases, you can run tasks until a condition is met. A test that waits for
a condition to be met is easier to understand and debug than a test that waits
for all tasks to run.

```cpp
int g_condition = false;

base::RunLoop run_loop;
base::ThreadPool::PostTask(FROM_HERE, {}, base::BindOnce(
    [] (base::OnceClosure closure) {
        g_condition = true;
        std::move(quit_closure).Run();
    }, run_loop.QuitClosure()));

// Runs tasks until the quit closure is invoked.
run_loop.Run();

EXPECT_TRUE(g_condition);
```

## Your question hasn't been answered?

 1. Check the main [Threading and Tasks](threading_and_tasks.md) docs.
 2. Ping
[scheduler-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/scheduler-dev).
