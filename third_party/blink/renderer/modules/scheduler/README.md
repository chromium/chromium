# `renderer/core/scheduler`

This directory contains the [Main Thread Scheduling
APIs](https://github.com/WICG/main-thread-scheduling) (`window.scheduler`).

**Note:** These APIs are experimental and the details are still in flux.

## APIs Implemented

`scheduler.postTask()`[[explainer](https://github.com/WICG/main-thread-scheduling/blob/master/PrioritizedPostTask.md), [design doc](https://docs.google.com/document/d/1xU7HyNsEsbXhTgt0ZnXDbeSXm5-m5FzkLJAT6LTizEI/edit#heading=h.iw2lczs6xwe6)]

## `scheduler.postTask()`

For the full API shape and general design, please refer to the (active)
[design doc](https://docs.google.com/document/d/1xU7HyNsEsbXhTgt0ZnXDbeSXm5-m5FzkLJAT6LTizEI/edit#heading=h.iw2lczs6xwe6).

### API Overview

The `postTask()` API allows developers to schedule tasks with a native scheduler
(`window.scheduler`), at a specific priority. The API is based on prioritized
task queues, which is a common scheduling paradigm.

The scheduler maintains a set of global task queues&mdash;one of each
priority&mdash;which developers can interact with directly via
`scheduler.getTaskQueue()` or indirectly through `scheduler.postTask()`.

Tasks are created with `scheduler.postTask(foo)` or `taskQueue.postTask(foo)`.
Initially, tasks are in a "pending" state, and transition to "running",
"canceled", or "completed" as illustrated in the following diagram:

![Task Lifecycle](images/task_lifecycle.png)

Tasks can be posted with an optional delay, which has similar behavior to
`setTimeout()`.

Tasks can be moved between task queues with `taskQueue.task(task)`.

### Code Overview and Blink Scheduler Integration

#### DOMScheduler

The `DOMScheduler` is per-document, and observes document lifecycle changes via
`ContextLifecycleObserver`. When the context is destroyed, the `DOMScheduler` stops
running tasks.

The `DOMScheduler`'s primary function is to maintain DOMTaskQueues. When the
`DOMScheduler` is created (when first accessed via `window.scheduler`), it creates
a set of *global* `DOMTaskQueues`, one of each priority.  Global here means
document-global, meaning all document script can access these via
`window.scheduler`. This is opposed to *custom* task queues, which developers
will be able to create via `new TaskQueue()` (WIP).


#### DOMTaskQueues

Each `DOMTaskQueue` wraps a `WebSchedulingTaskQueue`, which is the interface to the
Blink scheduler.

The `WebSchedulingTaskQueue` is created through the document's `FrameScheduler`,
and is created with a `WebSchedulingPriority`. The latter is used by the Blink
scheduler to determine the priority of the underlying
[base::sequence_manager::TaskQueue](https://cs.chromium.org/chromium/src/base/task/sequence_manager/task_queue.h).

The `DOMTaskQueue` owns the `WebSchedulingTaskQueue`, and its lifetime is tied to
that of the `Document` associated with the `DOMScheduler`.

The `WebSchedulingTaskQueue` exposes a `TaskRunner`, which the `DOMTaskQueue` uses
to post tasks to the Blink scheduler.


#### DOMTasks

A `DOMTask` is a wrapper for a callback, its arguments, its return value (for
`task.result`), and a
[`TaskHandle`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h?sq=package:chromium&g=0&l=22),
which allows the `DOMTask` to be canceled with `task.cancel()`.

The Blink scheduler handles the actual task scheduling. A `DOMTask`'s callback is
scheduled to run by posting it a task to a `TaskRunner`&mdash;just as all tasks
in Blink are. The `TaskRunner` is obtained through the `WebSchedulingTaskQueue`,
which is owned by the `DOMTaskQueue` that the `DOMTask` is posted to.

TODO(shaseley): Add a diagram for the relation between all the different task
queues.

#### Task Run Order

The API uses the Blink scheduler to run its tasks, and each
`WebSchedulingPriority` maps to a
[base::sequence_manager::TaskQueue::QueuePriority](https://cs.chromium.org/chromium/src/base/task/sequence_manager/task_queue.h?sq=package:chromium&g=0&l=76).

There is no intervention from the scheduling API to enforce a certain
order&mdash;the order that tasks run is strictly determined by the Blink
scheduler based on the `WebSchedulingPriority`.

By default, the Blink scheduler has an anti-starvation mechanism that will
occasionally prioritize lower priority tasks over higher priority tasks. This
can be disabled with
[BlinkSchedulerDisableAntiStarvationForPriorities](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/scheduler/common/features.h?q=BlinkSchedulerDisa&sq=package:chromium&g=0&l=173),
which we are relying on to achieve a static ordering (highest to lowest
priority) of tasks.

#### Detached Documents

The API is modeled after setTimeout and setInterval with regards to detached
documents. When a document is detached, any queued tasks will be prevented from
running, and future calls to postTask through a cached DOMScheduler or DOMTaskQueue
will be no-ops and return null.
