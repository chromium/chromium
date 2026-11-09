---
name: ios/invalidate-timers-in-teardown
description: In iOS classes that own timers, invalidate every NSTimer and stop every base::OneShotTimer or base::RepeatingTimer in the explicit teardown method (-disconnect, -stop, or Shutdown), not in -dealloc
rubric: safety
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Deallocation and Deterministic Cleanup
    quote: 'Classes that own timers stop them in their teardown method: call `-invalidate` on every `NSTimer` (a scheduled `NSTimer` retains its target until invalidated, so it keeps the object alive and keeps firing) and `Stop()` on `base::OneShotTimer` and `base::RepeatingTimer` members.'
---

**Rule.** When an `//ios` class owns a timer, stop it in the class's explicit
teardown method (`-disconnect` for mediators, `-stop` for coordinators,
`Shutdown()` for keyed services, or the equivalent): call `-invalidate` on every
`NSTimer` and set the ivar to `nil`, and call `Stop()` on every
`base::OneShotTimer`, `base::RetainingOneShotTimer`, and `base::RepeatingTimer`
member. Do not rely on `-dealloc` or the C++ destructor to stop timers that
touch models, consumers, or services.

**Why.** A scheduled `NSTimer` retains its target until it is invalidated, so
`-dealloc` never runs while the timer is live: the object stays alive and the
timer keeps firing after the feature is dismissed, often dereferencing services
that were already torn down. `base` timers do not retain their owner, but a
timer that fires between teardown and destruction still runs against cleared
dependencies. See
`ios/best_practices.md#deallocation-and-deterministic-cleanup`.

## Examples

Bad:

```cpp
@implementation FooMediator {
  NSTimer* _refreshTimer;
}

- (void)disconnect {
  _fooService = nullptr;
  // BAD: `_refreshTimer` still retains `self` and keeps firing.
}

- (void)dealloc {
  [_refreshTimer invalidate];  // BAD: Never reached while the timer is live.
}
@end
```

Good:

```cpp
@implementation FooMediator {
  NSTimer* _refreshTimer;
}

- (void)disconnect {
  [_refreshTimer invalidate];
  _refreshTimer = nil;
  _fooService = nullptr;
}
@end
```
