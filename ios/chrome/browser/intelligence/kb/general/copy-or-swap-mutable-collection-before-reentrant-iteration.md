---
name: ios/copy-or-swap-mutable-collection-before-reentrant-iteration
description: When iterating or draining mutable collections where callbacks or tasks can cause re-entrant mutation, snapshot the collection with -copy or drain it upfront with std::exchange instead of aliasing to NSArray*
rubric: correctness
severity: should_fix
---

**Rule.** Do not assign a mutable Objective-C collection (`NSMutableArray`,
`NSMutableDictionary`, `NSMutableSet`) to an immutable pointer (`NSArray*`,
`NSDictionary*`, `NSSet*`) assuming it creates an independent snapshot. When
iterating over a collection where callbacks, observers, or executed tasks can
cause re-entrant modifications, explicitly snapshot the collection using
`-[NSObject copy]` (or `mutableCopy`) or swap it out with a fresh container
using `std::exchange` before enumeration. When draining a pending queue where
unready items remain deferred, swap the queue upfront with `std::exchange` and
re-append deferred items to the new container rather than accumulating executed
items and calling `-[NSMutableArray removeObjectsInArray:]`.

**Why.** In Objective-C, assigning a mutable collection to an immutable pointer
variable only copies the object pointer, aliasing the same underlying mutable
instance. If re-entrant callbacks or tasks run during enumeration mutate the
collection, fast enumeration will throw an `NSGenericException` ('collection was
mutated while being enumerated') or corrupt iterator state, while bulk removal
via `-[NSMutableArray removeObjectsInArray:]` performs an O(N * M) scan instead
of detaching items in O(1) time with `std::exchange`.

## Examples

Bad:

```cpp
// Bad: assigning to NSArray* only aliases the pointer, not the contents.
NSArray<TaskRequest*>* pendingTasks = _pendingTasks;
for (TaskRequest* task in pendingTasks) {
  [task execute];  // Re-entrant task scheduling throws NSGenericException.
}

// Bad: accumulating executed tasks and removing them in bulk.
NSMutableArray<TaskRequest*>* executedTasks = [NSMutableArray array];
for (TaskRequest* task in _pendingTasks) {
  if (task.minimumStage <= currentStage) {
    [task execute];
    [executedTasks addObject:task];
  }
}
[_pendingTasks removeObjectsInArray:executedTasks];
```

Good:

```cpp
// Good: swap out the queue with a fresh container before iteration and
// re-append deferred items.
NSMutableArray<TaskRequest*>* pendingTasks =
    std::exchange(_pendingTasks, [NSMutableArray new]);
for (TaskRequest* task in pendingTasks) {
  if (task.minimumStage <= currentStage) {
    [task execute];
  } else {
    [_pendingTasks addObject:task];
  }
}

// ...or explicitly snapshot the collection before enumerating:
NSArray<TaskRequest*>* snapshot = [_pendingTasks copy];
for (TaskRequest* task in snapshot) {
  [task execute];
}
```
