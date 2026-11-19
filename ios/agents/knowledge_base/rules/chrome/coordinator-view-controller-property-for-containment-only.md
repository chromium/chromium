---
name: ios/coordinator-view-controller-property-for-containment-only
description: Do not use the public view controller property of a coordinator which is exposed for containment for any other purpose.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/coordinators.md
    heading: View Controller Containment
    quote: '1. **Child coordinator exposes its view controller**: The child coordinator declares a read-only `viewController` property. In `-start`, it creates and wires its `viewController` and `mediator` without inserting the view into a parent view hierarchy.'
  - path: ios/architecture/coordinators.md
    heading: Subclassing `ChromeCoordinator`
    quote: When a coordinator's view controller is contained as a child view controller inside a parent view hierarchy rather than presented modally, the coordinator may also expose a read-only **`viewController`** property (see View Controller Containment).
---

**Rule.** When a coordinator's view controller is contained rather than
presented, the child coordinator exposes it through a read-only `viewController`
property. After starting the child, the parent coordinator's only use of that
property is to assign it to a slot property on the parent's own view controller,
which performs the containment. Don't use it to configure the child's view
controller in any other way: don't set its properties or call its methods. Don't
add a public property exposing a coordinator's view controller for any other
purpose. Don't pass a view controller into a coordinator to be contained;
containment is always managed by the parent coordinator.

**Why.** The implementation details of the feature, including its view
controller, are not available for code outside of the coordinator to interact
with. In the specific case of view controller containment, a coordinator may
expose its view controller for the very narrow role of being placed inside a
containing view controller, and not for any other purpose. See
`ios/architecture/coordinators.md#view-controller-containment` and
`ios/architecture/coordinators.md#subclassing-chromecoordinator`.

## Examples

Bad:

```cpp
// parent_coordinator.mm
- (void)start {
  _viewController = [[ParentContainerViewController alloc] init];
  _childCoordinator = [[ChildCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];
  [_childCoordinator start];

  // Reaching through viewController to configure the child view controller, or
  // passing a view controller into a coordinator to be contained.
  _childCoordinator.viewController.overrideUserInterfaceStyle =
      UIUserInterfaceStyleDark;
  ChildViewController* childViewController = [[ChildViewController alloc] init];
  _childCoordinator.containedViewController = childViewController;
}
```

Good:

```cpp
// child_coordinator.h
@interface ChildCoordinator : ChromeCoordinator
// Exposed solely so the parent coordinator can pass it to a container view
// controller.
@property(nonatomic, strong, readonly) UIViewController* viewController;
@end

// parent_coordinator.mm
- (void)start {
  _viewController = [[ParentContainerViewController alloc] init];
  _childCoordinator = [[ChildCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];
  [_childCoordinator start];
  _viewController.childContentViewController =
      _childCoordinator.viewController;
}
```
