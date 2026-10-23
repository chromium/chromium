---
name: ios/use-chromium-task-runner-not-grand-central-dispatch
description: When dispatching asynchronous work, prefer Chromium TaskRunners over Grand Central Dispatch, document extension GCD usage, and avoid high-priority queues for non-urgent tasks
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Concurrency Best Practices
    quote: Prefer `base::SequencedTaskRunner` and `PostTask` / `PostDelayedTask` over `dispatch_async` unless an iOS system API requires a `dispatch_queue_t`.
---

**Rule.** Post asynchronous and background tasks using Chromium task runners
(`base::SequencedTaskRunner`,
`base::SequencedTaskRunner::GetCurrentDefault()->PostTask`, or
`base::ThreadPool`) rather than Grand Central Dispatch (`dispatch_async`). When
Grand Central Dispatch must be used (because an iOS system API requires a
`dispatch_queue_t`, or in app extensions and shared extension code in
`ios/chrome/common/` where Chromium task runners are unavailable), document the
rationale in an explanatory comment, and do not schedule non-time-sensitive
background operations on high-priority queues (`DISPATCH_QUEUE_PRIORITY_HIGH` or
`QOS_CLASS_USER_INTERACTIVE`); use low or default priority
(`DISPATCH_QUEUE_PRIORITY_LOW`, `DISPATCH_QUEUE_PRIORITY_DEFAULT`, or
`QOS_CLASS_UTILITY` / `QOS_CLASS_BACKGROUND`).

**Why.** Chromium TaskRunners integrate directly with Chromium's task
scheduling, message loops, threading watchdog, shutdown sequencing, and test
environments (such as TaskEnvironment). Grand Central Dispatch bypasses
Chromium's scheduler, hinders test deterministic control, and risks thread
exhaustion. When GCD is required because Chrome threads are not initialized
(such as in app extensions), documenting the rationale prevents confusion, while
avoiding high-priority queues prevents preempting interactive UI work. See
`ios/best_practices.md#concurrency-best-practices`.

## Examples

Bad:

```cpp
// Anti-pattern 1: Using GCD on the main queue instead of Chromium task runners
__weak __typeof(self) weakSelf = self;
dispatch_async(dispatch_get_main_queue(), ^{
  [weakSelf startEditCallback];
});

// Anti-pattern 2: Undocumented GCD usage in extension or shared code
dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
  PutSearchImageCommandInNSUserDefaults(command, imageData);
  dispatch_async(dispatch_get_main_queue(), completion);
});

// Anti-pattern 3: Using high priority for non-time-sensitive background tasks
dispatch_queue_t priorityQueue =
    dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0ul);
dispatch_async(priorityQueue, ^{
  UMA_HISTOGRAM_ENUMERATION(kHistogram, status);
});
```

Good:

```cpp
// Pattern 1: Prefer Chromium TaskRunner with base::BindOnce in browser code
__weak __typeof(self) weakSelf = self;
// Defer until the presentation animation has finished.
base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
    FROM_HERE, base::BindOnce(^{
      [weakSelf startEditCallback];
    }));

// Pattern 2: Document why GCD is used where Chrome threads are unavailable
// Use standard iOS GCD as Chrome task runners are not available in extensions.
dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
  PutSearchImageCommandInNSUserDefaults(command, imageData);
  dispatch_async(dispatch_get_main_queue(), completion);
});

// Pattern 3: Use default or low priority for non-urgent GCD tasks
dispatch_queue_t queue =
    dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0ul);
dispatch_async(queue, ^{
  UMA_HISTOGRAM_ENUMERATION(kHistogram, status);
});
```
