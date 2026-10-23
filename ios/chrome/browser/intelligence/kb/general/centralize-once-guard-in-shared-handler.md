---
name: ios/centralize-once-guard-in-shared-handler
description: When racing asynchronous callbacks or timeouts to invoke a shared handler, guard via an instance variable in the handler instead of local __block flags
rubric: safety
severity: should_fix
---

**Rule.** When racing multiple asynchronous completion callbacks or a timeout to
invoke a shared delegate or completion callback, store the single-invocation
status flag as an instance variable on the owning class (or rely on a shared
cancelable watchdog timer) rather than using a local `__block BOOL` variable.
Centralize the once-only guard check and state update inside the shared
completion helper method rather than duplicating boolean checks across
individual callback blocks, keeping callback closures stateless, linear, and
safely bound to `weakSelf`.

**Why.** Sharing local `__block` variables across asynchronous Objective-C
blocks and C++ `base::BindOnce` closures introduces memory lifetime hazards and
crashes if callbacks outlive the initiating frame or race across sequences.
Using an instance variable ties completion state lifetime to the owning object,
allowing callbacks weakly bound to `self` to safely no-op if destroyed, while
centralizing the guard inside the shared completion helper method prevents logic
duplication and guarantees that all completion paths are consistently protected
against multiple execution.

## Examples

Bad:

```cpp
- (void)fetchCapability:(id<SystemIdentity>)identity {
  __block BOOL callbackInvoked = NO;
  __weak __typeof(self) weakSelf = self;
  auto timeoutBlock = ^{
    if (callbackInvoked) {
      return;
    }
    callbackInvoked = YES;
    [weakSelf delegateFetchCompletedWithResult:Result::kTimeout];
  };
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(timeoutBlock), kTimeout);

  manager->Fetch(identity, base::BindOnce(^(Result result) {
    if (callbackInvoked) {
      return;
    }
    callbackInvoked = YES;
    [weakSelf delegateFetchCompletedWithResult:result];
  }));
}
```

Good:

```cpp
@implementation CapabilityFetcher {
  BOOL _callbackInvoked;
}

- (void)fetchCapability:(id<SystemIdentity>)identity {
  CHECK(!_callbackInvoked);
  __weak __typeof(self) weakSelf = self;
  // Enforce an upper bound on capability fetch latency.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(^{
        [weakSelf delegateFetchCompletedWithResult:Result::kTimeout];
      }),
      kTimeout);

  manager->Fetch(
      identity,
      base::BindOnce(^(Result result) {
        [weakSelf delegateFetchCompletedWithResult:result];
      }));
}

- (void)delegateFetchCompletedWithResult:(Result)result {
  if (_callbackInvoked) {
    return;
  }
  _callbackInvoked = YES;
  [_delegate capabilityFetcher:self didFetchResult:result];
}
@end
```
