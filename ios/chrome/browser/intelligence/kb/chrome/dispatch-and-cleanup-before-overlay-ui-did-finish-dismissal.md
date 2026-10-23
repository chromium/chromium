---
name: ios/dispatch-and-cleanup-before-overlay-ui-did-finish-dismissal
description: When invoking a delegate or completion callback that can synchronously deallocate the caller (such as OverlayUIDidFinishDismissal in OverlayRequestCoordinator subclasses), finish all command dispatches, state updates, and ivar accesses first
rubric: safety
severity: should_fix
---

**Rule.** When invoking a delegate or completion notification that can
synchronously deallocate the caller (such as `OverlayUIDidFinishDismissal(...)`
in `OverlayRequestCoordinator` subclasses), perform all command dispatches,
state updates, and instance variable access before invoking that notification.
Additionally, keep required command handlers and state valid across teardown
initiation (such as `-stopAnimated:`) until dismissal completes, rather than
prematurely resetting them before the dismissal completion callback runs.

**Why.** Invoking a dismissal-completion delegate method such as
`OverlayUIDidFinishDismissal` notifies the owner that UI has finished
dismissing, which can immediately advance the presentation queue and deallocate
the coordinator. Accessing ivars or dispatching commands on `self` after that
call causes a use-after-free crash, while prematurely `nil`-clearing handlers
prevents required cleanup dispatches during dismissal.

## Examples

Bad:

```cpp
- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  self.alertViewController = nil;
  self.presenter = nil;
  self.delegate->OverlayUIDidFinishDismissal(self.requestId);

  // Anti-pattern: Accessing ivars or dispatching commands after
  // `OverlayUIDidFinishDismissal` causes use-after-free on deallocated `self`.
  if (IsFeatureEnabled()) {
    [_pageInfoHandler hidePageInfo];
  }
}
```

Good:

```cpp
- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  self.alertViewController = nil;
  self.presenter = nil;
  if (IsFeatureEnabled()) {
    [_pageInfoHandler hidePageInfo];
  }
  _pageInfoHandler = nil;
  // Delegate notification deallocates coordinator; invoke it last.
  self.delegate->OverlayUIDidFinishDismissal(self.requestId);
}
```
