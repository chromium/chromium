---
name: ios/reset-scoped-ui-blocker-on-ui-teardown
description: When managing std::unique_ptr<ScopedUIBlocker>, bind it directly into asynchronous completion callbacks via base::BindOnce, or explicitly call .reset() during UI teardown before scene deallocation
rubric: safety
severity: should_fix
---

**Rule.** When guarding an asynchronous operation with `ScopedUIBlocker`,
transfer ownership of `std::unique_ptr<ScopedUIBlocker>` directly into the
completion callback using `base::BindOnce` and `std::move` instead of storing it
as an instance variable on the enclosing class. If a blocker must be retained
across multi-step UI flows as an instance variable, explicitly call `.reset()`
during UI teardown (such as in `SceneAgent` `-sceneStateDidDisableUI:`,
coordinator `-stop`, or mediator `-disconnect`) before the target `SceneState`
deallocates.

**Why.** Binding the scoped blocker directly into the completion callback ties
its lifetime to the asynchronous operation via RAII, preventing unnecessary
class ivars and ensuring the UI unblocks even if the callback is dropped. For
persistent ivars, explicitly resetting before UI teardown prevents destructor
`CHECK` failures caused by `ScopedUIBlocker` referencing an already-deallocated
`SceneState`.

## Examples

Bad:

```cpp
// Bad: Storing ScopedUIBlocker as an ivar for an asynchronous operation
@implementation MySceneAgent {
  std::unique_ptr<ScopedUIBlocker> _applicationUIBlocker;
}
- (void)fetchCapabilities {
  _applicationUIBlocker = ScopedUIBlocker::AppScoped(
      self.sceneState, self.sceneState.profileState.appState);
  __weak __typeof(self) weakSelf = self;
  base::OnceClosure closure = base::BindOnce(
      [](__typeof(self) strong_self, NSArray<id<SystemIdentity>>* ids) {
        [strong_self didCompleteFetchWithIdentities:ids];
      },
      weakSelf, ids);
}
- (void)didCompleteFetchWithIdentities:(NSArray<id<SystemIdentity>>*)ids {
  _applicationUIBlocker.reset();  // Requires manual reset; leaks UI if callback drops.
}

// Bad: Failing to reset persistent ScopedUIBlocker ivar during UI teardown
- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  [self.sceneState removeObserver:self];
  // Anti-pattern: _applicationUIBlocker not reset; crashes if scene deallocates first.
}
```

Good:

```cpp
// Good: Transferring ownership into the completion closure via base::BindOnce
- (void)fetchCapabilities {
  std::unique_ptr<ScopedUIBlocker> applicationUIBlocker =
      ScopedUIBlocker::AppScoped(self.sceneState,
                                 self.sceneState.profileState.appState);
  __weak __typeof(self) weakSelf = self;
  base::OnceClosure closure = base::BindOnce(
      [](std::unique_ptr<ScopedUIBlocker> blocker,
         __typeof(self) strong_self,
         NSArray<id<SystemIdentity>>* ids) {
        [strong_self didCompleteFetchWithIdentities:ids];
      },
      std::move(applicationUIBlocker), weakSelf, ids);
}

// Good: Explicitly resetting persistent blocker ivar during UI teardown
- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  [self.sceneState removeObserver:self];
  _applicationUIBlocker.reset();
}
```
