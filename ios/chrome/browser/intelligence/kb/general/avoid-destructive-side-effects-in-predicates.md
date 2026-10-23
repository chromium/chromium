---
name: ios/avoid-destructive-side-effects-in-predicates
description: When implementing boolean predicate or query methods, do not drain, reset, or mutate collections using std::exchange or clear; inspect containers in-place
rubric: correctness
severity: should_fix
---

**Rule.** Do not drain, clear, reset, or mutate collections or state (e.g.,
using `std::exchange`, `.clear()`, or `-[NSMutableArray removeAllObjects]`)
inside predicate or query methods. Predicates and query methods (such as those
named `should...`, `is...`, `can...`, or `has...`) must be non-destructive and
idempotent; inspect or iterate over the collection directly in place.

**Why.** Callers expect predicate and query methods to be read-only inspection
operations that can be invoked repeatedly without altering program state.
Draining or clearing collections during a query produces destructive side
effects, discarding queued or cached elements and corrupting application state.

## Examples

Bad:

```cpp
- (BOOL)shouldDropTaskRequest:(TaskRequest*)task {
  NSMutableArray<TaskRequest*>* pendingTasks =
      std::exchange(_pendingTasks, [NSMutableArray new]);
  for (TaskRequest* pendingTask in pendingTasks) {
    if (pendingTask.gaiaID && ![pendingTask.gaiaID isEqualToString:task.gaiaID]) {
      return YES;
    }
  }
  return NO;
}
```

Good:

```cpp
- (BOOL)shouldDropTaskRequest:(TaskRequest*)task {
  for (TaskRequest* pendingTask in _pendingTasks) {
    if (pendingTask.gaiaID && ![pendingTask.gaiaID isEqualToString:task.gaiaID]) {
      return YES;
    }
  }
  return NO;
}
```
