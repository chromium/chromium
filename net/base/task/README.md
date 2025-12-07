# //net/base/task

This directory provides a mechanism for obtaining `base::SingleThreadTaskRunner`
instances that are integrated with task scheduling and prioritization system of
the embedder process (e.g network service).

## Overview

The primary API offered is:

```cpp
namespace net {

const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner(
    RequestPriority priority);

}  // namespace net
```

This function allows code (typically running on the network thread or
interacting closely with network operations) to post tasks with a specific
`net::RequestPriority`.

## Integration with Network Service Scheduler

The `NetworkServiceTaskScheduler` (located in `//services/network/scheduler/`) is
responsible for setting up and managing the actual task queues on the network
service thread, including a high-priority queue.

During its initialization (specifically in `SetupNetTaskRunners()`), the
`NetworkServiceTaskScheduler` populates
`net::internal::GetTaskRunnerGlobals().high_priority_task_runner` with the task
runner associated with its own high-priority queue.

This ensures that when `net::GetTaskRunner(net::HIGHEST)` is called, tasks
posted to the returned runner are routed to the network service's designated
high-priority processing queue.

## Usage

Components that need to schedule work on the network thread with specific
network-related priorities should use `net::GetTaskRunner()`. This helps ensure
that critical network tasks (like those with `net::HIGHEST` priority) are
processed appropriately by the network service's scheduler.

If `RequestPriority` is unavailable or you are unsure which `priority` should be
used, continue to use `base::SingleThreadTaskRunner::GetCurrentDefault()` as
usual. This is currently equivalent to calling `net::GetTaskRunner(priority)`
for any `priority` except `net::HIGHEST`.

Task execution order is not guaranteed if you post tasks to different queues. If
you need posted tasks to be executed in order, use the same task runner.

The mapping of `RequestPriority` to underlying `TaskQueue` is subject to change.
Please do not write code that depends on this mapping.
