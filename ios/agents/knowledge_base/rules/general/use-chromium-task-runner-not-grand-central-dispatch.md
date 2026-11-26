---
name: ios/use-chromium-task-runner-not-grand-central-dispatch
description: Post asynchronous, delayed, and background tasks with Chromium task runners instead of Grand Central Dispatch, and comment why when GCD is required
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Concurrency Best Practices
    quote: Prefer `base::SequencedTaskRunner` and `PostTask` / `PostDelayedTask` over `dispatch_async` unless an iOS system API requires a `dispatch_queue_t`.
---

**Rule.** Post asynchronous, delayed, and background tasks using Chromium task
runners (`base::SequencedTaskRunner`, `PostTask` / `PostDelayedTask`, or
`base::ThreadPool`) rather than Grand Central Dispatch (`dispatch_async`,
`dispatch_after`). GCD is only permitted when an iOS system API takes a
`dispatch_queue_t`, or in app extensions and shared extension targets under
`ios/chrome/common/` where Chromium task runners are unavailable. When GCD is
required, add a comment explaining both why the work is asynchronous and why GCD
is used.

**Why.** Chromium task runners (`base::SequencedTaskRunner`, `base::ThreadPool`)
integrate with Chromium's sequence-bound threading model (`SEQUENCE_CHECKER`)
and deterministic test scheduling (`base::test::TaskEnvironment`), whereas
`dispatch_async` and `dispatch_after` bypass Chromium's task scheduler.
Documenting required GCD calls clarifies why the task is asynchronous and why a
Chromium task runner could not be used. See
`ios/best_practices.md#concurrency-best-practices`.

## Examples

Bad:

```cpp
// Anti-pattern 1: Using GCD on the main queue instead of Chromium task runners
__weak __typeof(self) weakSelf = self;
dispatch_async(dispatch_get_main_queue(), ^{
  [weakSelf didFinishUpdate];
});

// Anti-pattern 2: Undocumented GCD usage in extension or shared code
dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
  WriteSharedExtensionPayload(command, imageData);
  dispatch_async(dispatch_get_main_queue(), completion);
});
```

Good:

```cpp
// Pattern 1: Prefer Chromium TaskRunner with base::BindOnce in browser code
__weak __typeof(self) weakSelf = self;
// Defer to avoid mutating the observed model synchronously inside its
// observer callback.
base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
    FROM_HERE, base::BindOnce(^{
      [weakSelf didFinishUpdate];
    }));

// Pattern 2: Document why the work is asynchronous and why GCD is used
// Write the shared App Group payload off the main thread to avoid blocking UI;
// use GCD because Chromium task runners are unavailable in app extensions.
dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
  WriteSharedExtensionPayload(command, imageData);
  dispatch_async(dispatch_get_main_queue(), completion);
});
```
