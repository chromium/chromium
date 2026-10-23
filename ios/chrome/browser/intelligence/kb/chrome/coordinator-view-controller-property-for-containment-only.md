---
name: ios/coordinator-view-controller-property-for-containment-only
description: When coordinating UI flows, encapsulate view controller creation in coordinators; do not instantiate duplicate child controllers, expose mutable view controllers, or configure properties through coordinator.viewController
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/coordinators.md
    heading: Subclassing ChromeCoordinator
    quote: When a coordinator's view controller is contained as a child view controller inside a parent view hierarchy rather than presented modally, the coordinator may also expose a read-only viewController property.
---

**Rule.** When presenting a UI flow or child feature, encapsulate view
controller creation, configuration, and presentation inside its coordinator; do
not instantiate child view controllers directly in parent callers. Coordinators
must not expose internal view controllers as mutable public properties. If a
child view controller is contained within a parent view hierarchy rather than
presented modally, the coordinator may expose a read-only `viewController`
property for containment. Do not configure a child view controller by reaching
through `coordinator.viewController.<property>`; expose configuration properties
or methods on the coordinator itself and forward them internally to the view
controller. Additionally, when transferring or delegating view controller
ownership to a child or dedicated coordinator, do not continue allocating
duplicate `[[...ViewController alloc] init]` instances or strongly retaining
them in parent or consumer coordinators; obtain, pass, or weakly reference the
existing view controller instance managed by the owning coordinator.

**Why.** In Chrome for iOS architecture, coordinators manage presentation,
navigation, and view controller lifecycles. Instantiating view controllers
directly in callers or mutating properties through `coordinator.viewController`
violates encapsulation, bypasses mediator and coordinator setup routines,
tightly couples callers to concrete view controller classes, and prevents
swapping view controller implementations. Furthermore, allocating duplicate view
controller instances instead of delegating to dedicated owning coordinators
breaks ownership boundaries, leading to state desynchronization where UI updates
are not reflected in the active view, container view hierarchy inconsistencies,
or memory leaks from redundant strong references. See
`ios/architecture/coordinators.md#view-controller-containment` and
`ios/architecture/coordinators.md#subclassing`.

## Examples

Bad:

```cpp
// Anti-pattern 1: Instantiating child controller directly or mutating via coordinator.viewController.
controller = [[AutofillAndPasswordsTableViewController alloc] initWithBrowser:_browser];
_browserLayoutCoordinator.viewController.browserViewController = viewController;

// Anti-pattern 2: Allocating a duplicate view controller instead of referencing
// the one managed by the owning coordinator.
@implementation TabGridCoordinator {
  BrowserLayoutViewController* _browserLayoutViewController;
}
- (void)setupContainerViewController:(UIViewController*)viewController {
  _browserLayoutViewController = [[BrowserLayoutViewController alloc] init];
}
```

Good:

```cpp
// Pattern 1: Delegate presentation and configuration to coordinator.
[self showAutofillAndPasswords];
_browserLayoutCoordinator.browserViewController = viewController;

// Pattern 2: Weakly reference the view controller managed by the owning
// coordinator.
@implementation TabGridCoordinator {
  __weak BrowserLayoutViewController* _browserLayoutViewController;
}
- (void)setupContainerViewController:(UIViewController*)viewController {
  _browserLayoutViewController =
      base::apple::ObjCCastStrict<BrowserLayoutViewController>(viewController);
}
```
