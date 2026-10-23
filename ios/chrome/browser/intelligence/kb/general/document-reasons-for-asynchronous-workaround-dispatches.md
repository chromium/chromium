---
name: ios/document-reasons-for-asynchronous-workaround-dispatches
description: When deferring execution asynchronously via PostTask, dispatch_async, or completion blocks, document the reentrancy, timing, or race-prevention rationale and avoid delay terminology unless using PostDelayedTask
rubric: readability
severity: should_fix
citations:
  - path: docs/threading_and_tasks.md
    heading: Posting a Task with a Delay
    quote: To post a task that must run once after a delay expires, use base::ThreadPool::PostDelayedTask*() or base::TaskRunner::PostDelayedTask().
  - path: codelabs/cpp101/README.md
    heading: 'Part 3: Threads and task runners'
    quote: '- `PostDelayedTask()` if you want to add a delay.'
  - path: codelabs/cpp101/README.md
    heading: Threading
    quote: '- PostDelayedTask() if you want to add a delay.'
---

**Rule.** When deferring execution asynchronously on the current sequence,
thread, or runloop (such as via
base::SequencedTaskRunner::GetCurrentDefault()->PostTask, base::BindPostTask,
dispatch_async, setTimeout, or modal dismissal completion blocks) rather than
executing synchronously inline, document why asynchronous deferral is necessary
in an explanatory comment. Detail the specific reentrancy hazard, API timing
consistency (e.g., matching asynchronous network fetch contracts on cache hits),
race condition, or UI framework lifecycle constraint (such as presentation
clashes during dismissal or table view cell updates during layout). A comment
never legitimizes a runloop hop used as a completion proxy: never `PostTask` or
`dispatch_async` hoping that an animation, view presentation, network fetch, or
disk operation has finished by the next runloop turn; chain the work from the
explicit completion block, `base::OnceCallback`, delegate callback, or observer
notification instead. Additionally, do not describe immediate PostTask
dispatches as occurring 'after a delay'; reserve delay terminology for explicit
delayed tasks (PostDelayedTask) and document the actual sequencing or
race-prevention intent.

**Why.** Asynchronous deferrals, macrotask yields, and completion-block delays
introduced to break reentrancy, avoid race conditions, preserve consistent
timing contracts (such as for cache hits in async APIs), or avoid framework
lifecycle clashes are non-obvious because direct synchronous calls appear
simpler. Without an explicit inline comment, maintainers and reviewers cannot
determine whether asynchronous dispatch is load-bearing, risking accidental
regression to synchronous execution. A comment cannot, however, turn a runloop
hop into a completion signal: runloop-turn timing is coincidental, so a task
posted in the hope that an animation, presentation, fetch, or disk operation has
finished causes race conditions and flaky tests regardless of how well it is
documented. Furthermore, describing immediate PostTask dispatches as running
'after a delay' misleads maintainers into assuming a timer or delay parameter
was intended, obscuring the actual sequencing rationale.

## Examples

Bad:

```cpp
// Pattern 1: Cache hit callback deferred without documenting async contract
if (cached_image) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageFetcher::InvokeCallbackWithFetchedImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(item),
                     std::move(callback), cached_image));
  return;
}

// Pattern 2: Framework lifecycle workaround deferred without rationale
base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
    FROM_HERE, base::BindOnce(^{ [delegate updateCellAtIndexPath:path]; }));

// Pattern 3: Immediate PostTask described misleadingly as 'after a delay' and
// used as a completion proxy for the presentation started above
[self presentViewController:viewController animated:YES completion:nil];
// Trigger feature destination after a delay once the view is presented.
base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
    FROM_HERE, base::BindOnce(^{ [weakSelf triggerFeatureDestination]; }));

// Pattern 4: Deferring to dismissal completion or next runloop without explanation
[self.baseViewController dismissViewControllerAnimated:YES
                                            completion:completionBlock];
```

Good:

```cpp
// Pattern 1: Document timing contract consistency and reentrancy prevention
if (cached_image) {
  // Execute asynchronously to match the timing contract of network fetches
  // and prevent re-entrant callbacks during cache hits.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageFetcher::InvokeCallbackWithFetchedImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(item),
                     std::move(callback), cached_image));
  return;
}

// Pattern 2: Document framework lifecycle or update warning constraints
// Avoids calling cellForRowAtIndexPath while table view updates cells.
base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
    FROM_HERE, base::BindOnce(^{ [delegate updateCellAtIndexPath:path]; }));

// Pattern 3: No comment legitimizes a runloop hop used as a completion proxy;
// chain the work from the explicit completion block instead
[self presentViewController:viewController
                   animated:YES
                 completion:^{
                   [weakSelf triggerFeatureDestination];
                 }];

// Pattern 4: Document modal presentation sequencing or runloop settling rationale
// Run callback only after dismissal completes to prevent presentation clashes.
[self.baseViewController dismissViewControllerAnimated:YES
                                            completion:completionBlock];
```
