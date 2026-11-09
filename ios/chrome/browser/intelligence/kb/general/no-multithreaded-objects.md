---
name: ios/no-multithreaded-objects
description: Do not make iOS objects thread-safe with locks, @synchronized, or atomic properties; keep objects bound to one sequence and move work across sequences with task posting
rubric: correctness
severity: blocking
citations:
  - path: ios/best_practices.md
    heading: Concurrency Best Practices
    quote: If you find yourself reaching for locks, mutexes, `@synchronized`, or `atomic` Objective-C properties, stop and redesign around sequence-bound objects and task posting.
---

**Rule.** Do not write multithreaded objects in `//ios`: do not protect shared
state with `@synchronized`, `atomic` Objective-C properties, `base::Lock`,
`std::mutex`, `NSLock`, or `os_unfair_lock`. Bind each object to the sequence it
was created on (verified with `SEQUENCE_CHECKER`), and move work across
sequences with task posting, passing copied inputs to the background task and
returning results through the reply of `PostTaskAndReply` or
`PostTaskAndReplyWithResult`.

**Why.** Chrome on iOS is designed around sequence-bound objects; locks and
atomic properties hide data races instead of removing them, and make teardown
order impossible to reason about. An `atomic` property only makes each
individual access atomic, so compound updates still race. See
`ios/best_practices.md#concurrency-best-practices`.

## Examples

Bad:

```cpp
@interface FooMediator ()
// BAD: `atomic` because `items` is written from a background sequence.
@property(atomic, copy) NSArray<FooItem*>* items;
@end

@implementation FooMediator

// BAD: Lock protecting state shared across sequences.
- (void)addItem:(FooItem*)item {
  @synchronized(self) {
    self.items = [self.items arrayByAddingObject:item];
  }
}

@end
```

Good:

```cpp
@implementation FooMediator {
  SEQUENCE_CHECKER(_sequenceChecker);
  NSArray<FooItem*>* _items;
}

// Only called on the main sequence; background results arrive through a
// `PostTaskAndReplyWithResult` reply.
- (void)addItem:(FooItem*)item {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _items = [_items arrayByAddingObject:item];
}

@end
```
