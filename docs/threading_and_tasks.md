# Threading and Tasks in Chrome

[TOC]

## Overview

Chromium is a very multithreaded product. We try to keep the UI as responsive as
possible, and this means not blocking the UI thread with any blocking I/O or
other expensive operations. Our approach is to use message passing as the way of
communicating between threads. We discourage locking and threadsafe
objects. Instead, objects live on only one thread, we pass messages between
threads for communication, and we use callback interfaces (implemented by
message passing) for most cross-thread requests.

### Threads

Every Chrome process has

* a main thread
   * in the browser process: updates the UI
   * in renderer processes: runs most of Blink
* an IO thread
   * in the browser process: handles IPCs and network requests
   * in renderer processes: handles IPCs
* a few more special-purpose threads
* and a pool of general-purpose threads

Most threads have a loop that gets tasks from a queue and runs them (the queue
may be shared between multiple threads).

### Tasks

A task is a `base::OnceClosure` added to a queue for asynchronous execution.

A `base::OnceClosure` stores a function pointer and arguments. It has a `Run()`
method that invokes the function pointer using the bound arguments. It is
created using `base::BindOnce`. (ref. [Callback<> and Bind()
documentation](callback.md)).

```
void TaskA() {}
void TaskB(int v) {}

auto task_a = base::BindOnce(&TaskA);
auto task_b = base::BindOnce(&TaskB, 42);
```

A group of tasks can be executed in one of the following ways:

* [Parallel](#Posting-a-Parallel-Task): No task execution ordering, possibly all
  at once on any thread
* [Sequenced](#Posting-a-Sequenced-Task): Tasks executed in posting order, one
  at a time on any thread.
* [Single Threaded](#Posting-Multiple-Tasks-to-the-Same-Thread): Tasks executed
  in posting order, one at a time on a single thread.
   * [COM Single Threaded](#Posting-Tasks-to-a-COM-Single-Thread-Apartment-STA_Thread-Windows_):
     A variant of single threaded with COM initialized.

### Prefer Sequences to Threads

**Sequenced execution mode is far preferred to Single Threaded** in scenarios
that require mere thread-safety as it opens up scheduling paradigms that
wouldn't be possible otherwise (sequences can hop threads instead of being stuck
behind unrelated work on a dedicated thread). Ability to hop threads also means
the thread count can dynamically adapt to the machine's true resource
availability (increased parallelism on bigger machines, avoids trashing
resources on smaller machines).

Many core APIs were recently made sequence-friendly (classes are rarely
thread-affine -- i.e. only when using thread-local-storage or third-party APIs
that do). But the codebase has long evolved assuming single-threaded contexts...
If your class could run on a sequence but is blocked by an overzealous use of
ThreadChecker/ThreadTaskRunnerHandle/SingleThreadTaskRunner in a leaf
dependency, consider fixing that dependency for everyone's benefit (or at the
very least file a blocking bug against https://crbug.com/675631 and flag your
use of base::CreateSingleThreadTaskRunnerWithTraits() with a TODO against your
bug to use base::CreateSequencedTaskRunnerWithTraits() when fixed).

Detailed documentation on how to migrate from single-threaded contexts to
sequenced contexts can be found [here](threading_and_tasks_faq.md#How-can-I-migrate-from-SingleThreadTaskRunner-to-SequencedTaskRunner_).

The discussion below covers all of these ways to execute tasks in details.

## Posting a Parallel Task

### Direct Posting to the Task Scheduler

A task that can run on any thread and doesn’t have ordering or mutual exclusion
requirements with other tasks should be posted using one of the
`base::PostTask*()` functions defined in
[`base/task/post_task.h`](https://cs.chromium.org/chromium/src/base/task/post_task.h).

```cpp
base::PostTask(FROM_HERE, base::BindOnce(&Task));
```

This posts tasks with default traits.

The `base::PostTask*WithTraits()` functions allow the caller to provide
additional details about the task via TaskTraits (ref.
[Annotating Tasks with TaskTraits](#Annotating-Tasks-with-TaskTraits)).

```cpp
base::PostTaskWithTraits(
    FROM_HERE, {base::TaskPriority::BEST_EFFORT, MayBlock()},
    base::BindOnce(&Task));
```

### Posting via a TaskRunner

A parallel
[`TaskRunner`](https://cs.chromium.org/chromium/src/base/task_runner.h) is an
alternative to calling `base::PostTask*()` directly. This is mainly useful when
it isn’t known in advance whether tasks will be posted in parallel, in sequence,
or to a single-thread (ref.
[Posting a Sequenced Task](#Posting-a-Sequenced-Task),
[Posting Multiple Tasks to the Same Thread](#Posting-Multiple-Tasks-to-the-Same-Thread)).
Since `TaskRunner` is the base class of `SequencedTaskRunner` and
`SingleThreadTaskRunner`, a `scoped_refptr<TaskRunner>` member can hold a
`TaskRunner`, a `SequencedTaskRunner` or a `SingleThreadTaskRunner`.

```cpp
class A {
 public:
  A() = default;

  void set_task_runner_for_testing(
      scoped_refptr<base::TaskRunner> task_runner) {
    task_runner_ = std::move(task_runner);
  }

  void DoSomething() {
    // In production, A is always posted in parallel. In test, it is posted to
    // the TaskRunner provided via set_task_runner_for_testing().
    task_runner_->PostTask(FROM_HERE, base::BindOnce(&A));
  }

 private:
  scoped_refptr<base::TaskRunner> task_runner_ =
      base::CreateTaskRunnerWithTraits({base::TaskPriority::USER_VISIBLE});
};
```

Unless a test needs to control precisely how tasks are executed, it is preferred
to call `base::PostTask*()` directly (ref. [Testing](#Testing) for less invasive
ways of controlling tasks in tests).

## Posting a Sequenced Task

A sequence is a set of tasks that run one at a time in posting order (not
necessarily on the same thread). To post tasks as part of a sequence, use a
[`SequencedTaskRunner`](https://cs.chromium.org/chromium/src/base/sequenced_task_runner.h).

### Posting to a New Sequence

A `SequencedTaskRunner` can be created by
`base::CreateSequencedTaskRunnerWithTraits()`.

```cpp
scoped_refptr<SequencedTaskRunner> sequenced_task_runner =
    base::CreateSequencedTaskRunnerWithTraits(...);

// TaskB runs after TaskA completes.
sequenced_task_runner->PostTask(FROM_HERE, base::BindOnce(&TaskA));
sequenced_task_runner->PostTask(FROM_HERE, base::BindOnce(&TaskB));
```

### Posting to the Current Sequence

The `SequencedTaskRunner` to which the current task was posted can be obtained
via
[`SequencedTaskRunnerHandle::Get()`](https://cs.chromium.org/chromium/src/base/threading/sequenced_task_runner_handle.h).

*** note
**NOTE:** it is invalid to call `SequencedTaskRunnerHandle::Get()` from a
parallel task, but it is valid from a single-threaded task (a
`SingleThreadTaskRunner` is a `SequencedTaskRunner`).
***

```cpp
// The task will run after any task that has already been posted
// to the SequencedTaskRunner to which the current task was posted
// (in particular, it will run after the current task completes).
// It is also guaranteed that it won’t run concurrently with any
// task posted to that SequencedTaskRunner.
base::SequencedTaskRunnerHandle::Get()->
    PostTask(FROM_HERE, base::BindOnce(&Task));
```

## Using Sequences Instead of Locks

Usage of locks is discouraged in Chrome. Sequences inherently provide
thread-safety. Prefer classes that are always accessed from the same sequence to
managing your own thread-safety with locks.

```cpp
class A {
 public:
  A() {
    // Do not require accesses to be on the creation sequence.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  void AddValue(int v) {
    // Check that all accesses are on the same sequence.
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    values_.push_back(v);
}

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // No lock required, because all accesses are on the
  // same sequence.
  std::vector<int> values_;
};

A a;
scoped_refptr<SequencedTaskRunner> task_runner_for_a = ...;
task_runner_for_a->PostTask(FROM_HERE,
                      base::BindOnce(&A::AddValue, base::Unretained(&a), 42));
task_runner_for_a->PostTask(FROM_HERE,
                      base::BindOnce(&A::AddValue, base::Unretained(&a), 27));

// Access from a different sequence causes a DCHECK failure.
scoped_refptr<SequencedTaskRunner> other_task_runner = ...;
other_task_runner->PostTask(FROM_HERE,
                            base::BindOnce(&A::AddValue, base::Unretained(&a), 1));
```

Locks should only be used to swap in a shared data structure that can be
accessed on multiple threads.  If one thread updates it based on expensive
computation or through disk access, then that slow work should be done without
holding on to the lock.  Only when the result is available should the lock be
used to swap in the new data.  An example of this is in PluginList::LoadPlugins
([`content/common/plugin_list.cc`](https://cs.chromium.org/chromium/src/content/
common/plugin_list.cc). If you must use locks,
[here](https://www.chromium.org/developers/lock-and-condition-variable) are some
best practices and pitfalls to avoid.

In order to write non-blocking code, many APIs in Chromium are asynchronous.
Usually this means that they either need to be executed on a particular
thread/sequence and will return results via a custom delegate interface, or they
take a `base::Callback<>` object that is called when the requested operation is
completed.  Executing work on a specific thread/sequence is covered in the
PostTask sections above.

## Posting Multiple Tasks to the Same Thread

If multiple tasks need to run on the same thread, post them to a
[`SingleThreadTaskRunner`](https://cs.chromium.org/chromium/src/base/single_thread_task_runner.h).
All tasks posted to the same `SingleThreadTaskRunner` run on the same thread in
posting order.

### Posting to the Main Thread or to the IO Thread in the Browser Process

To post tasks to the main thread or to the IO thread, use
`base::PostTaskWithTraits()` or get the appropriate SingleThreadTaskRunner using
`base::CreateSingleThreadTaskRunnerWithTraits`, supplying a `BrowserThread::ID`
as trait. For this, you'll also need to include
[`content/public/browser/browser_task_traits.h`](https://cs.chromium.org/chromium/src/content/public/browser/browser_task_traits.h).

```cpp
base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI}, ...);

base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::IO})
    ->PostTask(FROM_HERE, ...);
```

The main thread and the IO thread are already super busy. Therefore, prefer
posting to a general purpose thread when possible (ref.
[Posting a Parallel Task](#Posting-a-Parallel-Task),
[Posting a Sequenced task](#Posting-a-Sequenced-Task)).
Good reasons to post to the main thread are to update the UI or access objects
that are bound to it (e.g. `Profile`). A good reason to post to the IO thread is
to access the internals of components that are bound to it (e.g. IPCs, network).
Note: It is not necessary to have an explicit post task to the IO thread to
send/receive an IPC or send/receive data on the network.

### Posting to the Main Thread in a Renderer Process
TODO

### Posting to a Custom SingleThreadTaskRunner

If multiple tasks need to run on the same thread and that thread doesn’t have to
be the main thread or the IO thread, post them to a `SingleThreadTaskRunner`
created by `base::CreateSingleThreadTaskRunnerWithTraits`.

```cpp
scoped_refptr<SequencedTaskRunner> single_thread_task_runner =
    base::CreateSingleThreadTaskRunnerWithTraits(...);

// TaskB runs after TaskA completes. Both tasks run on the same thread.
single_thread_task_runner->PostTask(FROM_HERE, base::BindOnce(&TaskA));
single_thread_task_runner->PostTask(FROM_HERE, base::BindOnce(&TaskB));
```

*** note
**IMPORTANT:** You should rarely need this, most classes in Chromium require
thread-safety (which sequences provide) not thread-affinity. If an API you’re
using is incorrectly thread-affine (i.e. using
[`base::ThreadChecker`](https://cs.chromium.org/chromium/src/base/threading/thread_checker.h)
when it’s merely thread-unsafe and should use
[`base::SequenceChecker`](https://cs.chromium.org/chromium/src/base/sequence_checker.h)),
please consider fixing it instead of making things worse by also making your API thread-affine.
***

### Posting to the Current Thread

*** note
**IMPORTANT:** To post a task that needs mutual exclusion with the current
sequence of tasks but doesn’t absolutely need to run on the current thread, use
`SequencedTaskRunnerHandle::Get()` instead of `ThreadTaskRunnerHandle::Get()`
(ref. [Posting to the Current Sequence](#Posting-to-the-Current-Sequence)). That
will better document the requirements of the posted task. In a single-thread
task, `SequencedTaskRunnerHandle::Get()` is equivalent to
`ThreadTaskRunnerHandle::Get()`.
***

To post a task to the current thread, use [`ThreadTaskRunnerHandle`](https://cs.chromium.org/chromium/src/base/threading/thread_task_runner_handle.h).

```cpp
// The task will run on the current thread in the future.
base::ThreadTaskRunnerHandle::Get()->PostTask(
    FROM_HERE, base::BindOnce(&Task));
```

*** note
**NOTE:** It is invalid to call `ThreadTaskRunnerHandle::Get()` from a parallel
or a sequenced task.
***

## Posting Tasks to a COM Single-Thread Apartment (STA) Thread (Windows)

Tasks that need to run on a COM Single-Thread Apartment (STA) thread must be
posted to a `SingleThreadTaskRunner` returned by
`CreateCOMSTATaskRunnerWithTraits()`. As mentioned in [Posting Multiple Tasks to
the Same Thread](#Posting-Multiple-Tasks-to-the-Same-Thread), all tasks posted
to the same `SingleThreadTaskRunner` run on the same thread in posting order.

```cpp
// Task(A|B|C)UsingCOMSTA will run on the same COM STA thread.

void TaskAUsingCOMSTA() {
  // [ This runs on a COM STA thread. ]

  // Make COM STA calls.
  // ...

  // Post another task to the current COM STA thread.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TaskCUsingCOMSTA));
}
void TaskBUsingCOMSTA() { }
void TaskCUsingCOMSTA() { }

auto com_sta_task_runner = base::CreateCOMSTATaskRunnerWithTraits(...);
com_sta_task_runner->PostTask(FROM_HERE, base::BindOnce(&TaskAUsingCOMSTA));
com_sta_task_runner->PostTask(FROM_HERE, base::BindOnce(&TaskBUsingCOMSTA));
```

## Annotating Tasks with TaskTraits

[`TaskTraits`](https://cs.chromium.org/chromium/src/base/task/task_traits.h)
encapsulate information about a task that helps the task scheduler make better
scheduling decisions.

All `PostTask*()` functions in
[`base/task/post_task.h`](https://cs.chromium.org/chromium/src/base/task/post_task.h)
have an overload that takes `TaskTraits` as argument and one that doesn’t. The
overload that doesn’t take `TaskTraits` as argument is appropriate for tasks
that:
- Don’t block (ref. MayBlock and WithBaseSyncPrimitives).
- Prefer inheriting the current priority to specifying their own.
- Can either block shutdown or be skipped on shutdown (task scheduler is free to choose a fitting default).
Tasks that don’t match this description must be posted with explicit TaskTraits.

[`base/task/task_traits.h`](https://cs.chromium.org/chromium/src/base/task/task_traits.h)
provides exhaustive documentation of available traits. The content layer also
provides additional traits in
[`content/public/browser/browser_task_traits.h`](https://cs.chromium.org/chromium/src/content/public/browser/browser_task_traits.h)
to facilitate posting a task onto a BrowserThread.

Below are some examples of how to specify `TaskTraits`.

```cpp
// This task has no explicit TaskTraits. It cannot block. Its priority
// is inherited from the calling context (e.g. if it is posted from
// a BEST_EFFORT task, it will have a BEST_EFFORT priority). It will either
// block shutdown or be skipped on shutdown.
base::PostTask(FROM_HERE, base::BindOnce(...));

// This task has the highest priority. The task scheduler will try to
// run it before USER_VISIBLE and BEST_EFFORT tasks.
base::PostTaskWithTraits(
    FROM_HERE, {base::TaskPriority::USER_BLOCKING},
    base::BindOnce(...));

// This task has the lowest priority and is allowed to block (e.g. it
// can read a file from disk).
base::PostTaskWithTraits(
    FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
    base::BindOnce(...));

// This task blocks shutdown. The process won't exit before its
// execution is complete.
base::PostTaskWithTraits(
    FROM_HERE, {base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
    base::BindOnce(...));

// This task will run on the Browser UI thread.
base::PostTaskWithTraits(
    FROM_HERE, {content::BrowserThread::UI},
    base::BindOnce(...));
```

## Keeping the Browser Responsive

Do not perform expensive work on the main thread, the IO thread or any sequence
that is expected to run tasks with a low latency. Instead, perform expensive
work asynchronously using `base::PostTaskAndReply*()` or
`SequencedTaskRunner::PostTaskAndReply()`. Note that asynchronous/overlapped
I/O on the IO thread are fine.

Example: Running the code below on the main thread will prevent the browser from
responding to user input for a long time.

```cpp
// GetHistoryItemsFromDisk() may block for a long time.
// AddHistoryItemsToOmniboxDropDown() updates the UI and therefore must
// be called on the main thread.
AddHistoryItemsToOmniboxDropdown(GetHistoryItemsFromDisk("keyword"));
```

The code below solves the problem by scheduling a call to
`GetHistoryItemsFromDisk()` in a thread pool followed by a call to
`AddHistoryItemsToOmniboxDropdown()` on the origin sequence (the main thread in
this case). The return value of the first call is automatically provided as
argument to the second call.

```cpp
base::PostTaskWithTraitsAndReplyWithResult(
    FROM_HERE, {base::MayBlock()},
    base::BindOnce(&GetHistoryItemsFromDisk, "keyword"),
    base::BindOnce(&AddHistoryItemsToOmniboxDropdown));
```

## Posting a Task with a Delay

### Posting a One-Off Task with a Delay

To post a task that must run once after a delay expires, use
`base::PostDelayedTask*()` or `TaskRunner::PostDelayedTask()`.

```cpp
base::PostDelayedTaskWithTraits(
  FROM_HERE, {base::TaskPriority::BEST_EFFORT}, base::BindOnce(&Task),
  base::TimeDelta::FromHours(1));

scoped_refptr<base::SequencedTaskRunner> task_runner =
    base::CreateSequencedTaskRunnerWithTraits({base::TaskPriority::BEST_EFFORT});
task_runner->PostDelayedTask(
    FROM_HERE, base::BindOnce(&Task), base::TimeDelta::FromHours(1));
```

*** note
**NOTE:** A task that has a 1-hour delay probably doesn’t have to run right away
when its delay expires. Specify `base::TaskPriority::BEST_EFFORT` to prevent it
from slowing down the browser when its delay expires.
***

### Posting a Repeating Task with a Delay
To post a task that must run at regular intervals,
use [`base::RepeatingTimer`](https://cs.chromium.org/chromium/src/base/timer/timer.h).

```cpp
class A {
 public:
  ~A() {
    // The timer is stopped automatically when it is deleted.
  }
  void StartDoingStuff() {
    timer_.Start(FROM_HERE, TimeDelta::FromSeconds(1),
                 this, &MyClass::DoStuff);
  }
  void StopDoingStuff() {
    timer_.Stop();
  }
 private:
  void DoStuff() {
    // This method is called every second on the sequence that invoked
    // StartDoingStuff().
  }
  base::RepeatingTimer timer_;
};
```

## Cancelling a Task

### Using base::WeakPtr

[`base::WeakPtr`](https://cs.chromium.org/chromium/src/base/memory/weak_ptr.h)
can be used to ensure that any callback bound to an object is canceled when that
object is destroyed.

```cpp
int Compute() { … }

class A {
 public:
  A() : weak_ptr_factory_(this) {}

  void ComputeAndStore() {
    // Schedule a call to Compute() in a thread pool followed by
    // a call to A::Store() on the current sequence. The call to
    // A::Store() is canceled when |weak_ptr_factory_| is destroyed.
    // (guarantees that |this| will not be used-after-free).
    base::PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&Compute),
        base::BindOnce(&A::Store, weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void Store(int value) { value_ = value; }

  int value_;
  base::WeakPtrFactory<A> weak_ptr_factory_;
};
```

Note: `WeakPtr` is not thread-safe: `GetWeakPtr()`, `~WeakPtrFactory()`, and
`Compute()` (bound to a `WeakPtr`) must all run on the same sequence.

### Using base::CancelableTaskTracker

[`base::CancelableTaskTracker`](https://cs.chromium.org/chromium/src/base/task/cancelable_task_tracker.h)
allows cancellation to happen on a different sequence than the one on which
tasks run. Keep in mind that `CancelableTaskTracker` cannot cancel tasks that
have already started to run.

```cpp
auto task_runner = base::CreateTaskRunnerWithTraits(base::TaskTraits());
base::CancelableTaskTracker cancelable_task_tracker;
cancelable_task_tracker.PostTask(task_runner.get(), FROM_HERE,
                                 base::DoNothing());
// Cancels Task(), only if it hasn't already started running.
cancelable_task_tracker.TryCancelAll();
```

## Testing

To test code that uses `base::ThreadTaskRunnerHandle`,
`base::SequencedTaskRunnerHandle` or a function in
[`base/task/post_task.h`](https://cs.chromium.org/chromium/src/base/task/post_task.h), instantiate a
[`base::test::ScopedTaskEnvironment`](https://cs.chromium.org/chromium/src/base/test/scoped_task_environment.h)
for the scope of the test.

```cpp
class MyTest : public testing::Test {
 public:
  // ...
 protected:
   base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST(MyTest, MyTest) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::BindOnce(&A));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   base::BindOnce(&B));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&C), base::TimeDelta::Max());

  // This runs the (Thread|Sequenced)TaskRunnerHandle queue until it is empty.
  // Delayed tasks are not added to the queue until they are ripe for execution.
  base::RunLoop().RunUntilIdle();
  // A and B have been executed. C is not ripe for execution yet.

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::BindOnce(&D));
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, run_loop.QuitClosure());
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::BindOnce(&E));

  // This runs the (Thread|Sequenced)TaskRunnerHandle queue until QuitClosure is
  // invoked.
  run_loop.Run();
  // D and run_loop.QuitClosure() have been executed. E is still in the queue.

  // Tasks posted to task scheduler run asynchronously as they are posted.
  base::PostTaskWithTraits(FROM_HERE, base::TaskTraits(), base::BindOnce(&F));
  auto task_runner =
      base::CreateSequencedTaskRunnerWithTraits(base::TaskTraits());
  task_runner->PostTask(FROM_HERE, base::BindOnce(&G));

  // To block until all tasks posted to task scheduler are done running:
  base::TaskScheduler::GetInstance()->FlushForTesting();
  // F and G have been executed.

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTrait(),
      base::BindOnce(&H), base::BindOnce(&I));

  // This runs the (Thread|Sequenced)TaskRunnerHandle queue until both the
  // (Thread|Sequenced)TaskRunnerHandle queue and the TaskSchedule queue are
  // empty:
  scoped_task_environment_.RunUntilIdle();
  // E, H, I have been executed.
}
```

## Using TaskScheduler in a New Process

TaskScheduler needs to be initialized in a process before the functions in
[`base/task/post_task.h`](https://cs.chromium.org/chromium/src/base/task/post_task.h)
can be used. Initialization of TaskScheduler in the Chrome browser process and
child processes (renderer, GPU, utility) has already been taken care of. To use
TaskScheduler in another process, initialize TaskScheduler early in the main
function:

```cpp
// This initializes and starts TaskScheduler with default params.
base::TaskScheduler::CreateAndStartWithDefaultParams(“process_name”);
// The base/task/post_task.h API can now be used. Tasks will be // scheduled as
// they are posted.

// This initializes TaskScheduler.
base::TaskScheduler::Create(“process_name”);
// The base/task/post_task.h API can now be used. No threads // will be created
// and no tasks will be scheduled until after Start() is called.
base::TaskScheduler::GetInstance()->Start(params);
// TaskScheduler can now create threads and schedule tasks.
```

And shutdown TaskScheduler late in the main function:

```cpp
base::TaskScheduler::GetInstance()->Shutdown();
// Tasks posted with TaskShutdownBehavior::BLOCK_SHUTDOWN and
// tasks posted with TaskShutdownBehavior::SKIP_ON_SHUTDOWN that
// have started to run before the Shutdown() call have now completed their
// execution. Tasks posted with
// TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN may still be
// running.
```
## TaskRunner ownership (encourage no dependency injection)

TaskRunners shouldn't be passed through several components. Instead, the
components that uses a TaskRunner should be the one that creates it.

See [this example](https://codereview.chromium.org/2885173002/) of a
refactoring where a TaskRunner was passed through a lot of components only to be
used in an eventual leaf. The leaf can and should now obtain its TaskRunner
directly from
[`base/task/post_task.h`](https://cs.chromium.org/chromium/src/base/task/post_task.h).

Dependency injection of TaskRunners can still seldomly be useful to unit test a
component when triggering a specific race in a specific way is essential to the
test. For such cases the preferred approach is the following:

```cpp
class FooWithCustomizableTaskRunnerForTesting {
 public:

  void SetBackgroundTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

 private:
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_ =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
}
```

Note that this still allows removing all layers of plumbing between //chrome and
that component since unit tests will use the leaf layer directly.
