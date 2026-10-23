---
name: ios/coordinators-do-not-own-lifecycle
description: When managing coordinator lifecycles or workflow exits, drive teardown through parent coordinators, signal completion across exit branches, and encapsulate child teardown within dedicated cleanup helpers
rubric: correctness
severity: should_fix
citations:
  - path: ios/architecture/coordinators.md
    heading: Parent Ownership of Lifecycle
    quote: Because a coordinator is started once and destroyed after -stop, a coordinator must never call -start or -stop on itself, and -start and -stop must not be called repeatedly on the same instance.
  - path: ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h
    heading: The basic lifecycle methods for coordinators are -start and -stop
    quote: The basic lifecycle methods for coordinators are -start and -stop. These are blank template methods; child classes are expected to implement them and do not need to invoke the superclass methods.
---

**Rule.** Drive coordinator lifecycles and dismissals strictly through parent
coordinators: a child coordinator must never call `[self stop]`, and must not
dismiss its own view controller in response to a user action or exit branch
(such as a Done button, an alert action, or opening Settings) instead of
signaling the parent. Route teardown to the parent by invoking the feature's
hide command via a command handler (or a coordinator delegate callback for
parent-agnostic coordinators) so the parent invokes `-stop` and nils its
reference; the only place a coordinator dismisses its own view controller is
inside `-stop`, and that dismissal must be a safe no-op when UIKit has already
dismissed the view controller. Additionally, when stopping child coordinators,
mediators, or view controllers, centralize teardown in dedicated helper methods
(e.g., `[self stop<Child>Coordinator]`, `stopChildCoordinators`, or
`destroyViewControllersAndMediators`). Encapsulate the complete disconnection
and teardown of subordinate components within that helper, delegating directly
to it from `-stop` and dismissal handlers rather than splitting or duplicating
cleanup steps inline. Within teardown helpers, clear delegates, observers, and
handlers before calling `[child stop]`, and nil the reference afterward without
bypassing intermediate coordinators.

**Why.** Dismissing a view controller from an action handler, calling
`[self stop]`, or navigating away on an exit branch without signaling the parent
leaves parent coordinators holding strong references to stale coordinators,
bypassing coordinator teardown and leaking child resources; because `-stop` also
runs after UIKit-driven dismissals (swipe-down, Back), dismissal inside `-stop`
must tolerate an already-dismissed view controller. Dedicated teardown helpers
(like `stopChildCoordinators` or `stop<Child>Coordinator`) establish a single
authoritative source of truth ensuring subordinate coordinators, view
controllers, and mediators are fully dismantled. Splitting teardown statements
across external callers or duplicating individual child coordinator stops
directly in `-stop` breaks encapsulation and risks inconsistent cleanup.
Clearing delegates and handlers before `-stop` prevents re-entrant callbacks
during dismissal transitions, guarantees UI can be re-presented, and maintains
hierarchical encapsulation. See
`ios/architecture/coordinators.md#parent-ownership-of-lifecycle` and
`ios/architecture/coordinators.md` ("Handling UIKit-Driven Dismissals").

## Examples

Bad:

```cpp
// Anti-pattern 1: Coordinator stopping or dismissing itself on a user action
- (void)dismissButtonTapped {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  [self stop];
}

// Anti-pattern 2: Duplicating/splitting child stop inline instead of delegating to helper
- (void)stop {
  [_childCoordinator stop];
  _childCoordinator = nil;
  [self stopChildCoordinators];
}

// Anti-pattern 3: Stopping child coordinator before clearing delegates
- (void)stopAccountPickerCoordinator {
  [_accountPickerCoordinator stop];
  _accountPickerCoordinator.delegate = nil;
  _accountPickerCoordinator = nil;
}
```

Good:

```cpp
// Solution 1: Ask the parent to stop this feature via the command it handles
- (void)dismissButtonTapped {
  id<ChildCommands> childHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), ChildCommands);
  [childHandler hideChildFeature];
}

// Solution 2: Centralize all child teardown inside dedicated helper; dismissal
// in -stop is a safe no-op if UIKit already dismissed the view controller
- (void)stop {
  [self stopChildCoordinators];
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _viewController = nil;
}

// Solution 3: Clear delegates before stopping child and niling reference
- (void)stopAccountPickerCoordinator {
  _accountPickerCoordinator.delegate = nil;
  [_accountPickerCoordinator stop];
  _accountPickerCoordinator = nil;
}
```
