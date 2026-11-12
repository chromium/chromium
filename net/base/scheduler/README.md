# Net Task Scheduler

This directory contains the implementation for a task scheduler on Chromium's
network stack.

The scheduler introduces a `base::sequence_manager::SequenceManager` configured
with multiple prioritized task queues. The primary goal is to allow
high-priority network tasks (e.g., those critical for navigation) to execute
with precedence over lower-priority tasks, aiming to improve user-perceived
performance metrics like FCP and LCP.

## Design

For a detailed explanation of the design, motivations, and implementation plan,
please refer to the design document:
[go/task-scheduler-in-net](http://go/task-scheduler-in-net) (Google internal)

## Overview

The core mechanism involves:

1. Initializing a `SequenceManager` on the network thread.
2. Creating at least two `TaskQueue`s: one for high-priority tasks and one for
   default-priority tasks.
3. Exposing `TaskRunner`s associated with these queues.
4. Modifying relevant `PostTask` call sites in `//net` and `//services/network`
   to route tasks to the appropriate `TaskRunner` based on the task's conceptual
   priority (often derived from `net::RequestPriority`).

## Usage

The primary API offered for task posting is:

```cpp
namespace net {

const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner(
    RequestPriority priority);

}  // namespace net
```

Components that need to schedule work on the network thread with specific
network-related priorities should use `net::GetTaskRunner()`. This ensures
that critical network tasks (like those with `net::HIGHEST` priority) are
routed to high-priority queues when the task scheduler is active.

If `RequestPriority` is unavailable or you are unsure which `priority` should be
used, continue to use `base::SingleThreadTaskRunner::GetCurrentDefault()` as
usual. When the scheduler is disabled or not active in the current process,
`net::GetTaskRunner()` falls back to `base::SingleThreadTaskRunner::GetCurrentDefault()`.
Note that `net::GetTaskRunner()` reads process-global state and must only be
called on the network thread when the scheduler is active.

Task execution order is not guaranteed if you post tasks to different queues. If
you need posted tasks to be executed in order, use the same task runner.

The mapping of `RequestPriority` to underlying `TaskQueue` is subject to change.
Please do not write code that depends on this mapping.
