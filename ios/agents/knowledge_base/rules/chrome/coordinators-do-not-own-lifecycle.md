---
name: ios/coordinators-do-not-own-lifecycle
description: Drive coordinator teardown strictly through parent coordinators via hide commands or delegates rather than calling [self stop] or dismissing outside -stop
rubric: correctness
severity: should_fix
citations:
  - path: ios/architecture/coordinators.md
    heading: Parent Ownership of Lifecycle
    quote: Because a coordinator is started once and destroyed after `-stop`, **a coordinator must never call `-start` or `-stop` on itself**, and `-start` and `-stop` must not be called repeatedly on the same instance.
  - path: ios/architecture/coordinators.md
    heading: Handling UIKit-Driven Dismissals
    quote: Never call `[self stop]` in response to a UIKit dismissal. Instead, route the teardown request to the parent coordinator just like an explicit close button—by invoking the feature's hide command via a command handler (or calling a coordinator delegate method for parent-agnostic coordinators) so the parent stops and releases the child coordinator.
---

**Rule.** Drive coordinator lifecycles and dismissals strictly through parent
coordinators. A child coordinator must never call `[self stop]` or dismiss its
own view controller in response to a user action or exit branch (such as a Done
button, alert action, or Settings navigation). Instead, request teardown from
the parent via the feature's hide command (or a coordinator delegate method for
parent-agnostic coordinators) so the parent calls `-stop` and clears its
reference. A coordinator must only dismiss its view controller inside `-stop`,
using a call that is a safe no-op if UIKit already dismissed it (such as
messaging `_viewController.presentingViewController`).

**Why.** Calling `[self stop]` or dismissing a view controller without signaling
the parent leaves the parent holding a strong reference to a stopped or stale
child coordinator. Because `-stop` runs both for programmatic teardown and after
UIKit-driven dismissals (such as swipe-down or Back), view controller dismissal
in `-stop` must safely tolerate an already-dismissed view controller. See
`ios/architecture/coordinators.md#parent-ownership-of-lifecycle` and
`ios/architecture/coordinators.md#handling-uikit-driven-dismissals`.

## Examples

Bad:

```cpp
// Child coordinator dismissing and stopping itself on a user action:
- (void)dismissButtonTapped {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  [self stop];
}
```

Good:

```cpp
// Signal the parent via the feature's hide command, and dismiss in -stop via
// presentingViewController (a safe no-op if UIKit already dismissed it):
- (void)dismissButtonTapped {
  id<ChildCommands> childHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), ChildCommands);
  [childHandler hideChildFeature];
}

- (void)stop {
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _viewController = nil;
}
```
