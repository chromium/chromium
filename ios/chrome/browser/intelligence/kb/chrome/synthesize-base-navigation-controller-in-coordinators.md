---
name: ios/synthesize-base-navigation-controller-in-coordinators
description: In ChromeCoordinator subclasses, synthesize baseNavigationController when pushing to navigation stacks instead of downcasting or declaring duplicate ivars; omit for modal coordinators
rubric: readability
severity: should_fix
---

**Rule.** In `ChromeCoordinator` subclasses that present views by pushing onto
an existing navigation stack:

1. Declare `-initWithBaseNavigationController:browser:` as the designated
   initializer, mark `-initWithBaseViewController:browser:` as `NS_UNAVAILABLE`,
   explicitly synthesize the inherited property
   (`@synthesize baseNavigationController = _baseNavigationController;`), and
   assign `_baseNavigationController = navigationController;` in the
   initializer.
2. Use `self.baseNavigationController` or `_baseNavigationController` directly
   when pushing or presenting views; do not downcast `self.baseViewController`
   to `UINavigationController` (e.g., via
   `base::apple::ObjCCastStrict<UINavigationController>`).
3. Do not declare an independent private instance variable (such as
   `UINavigationController* _baseNavigationController;` in the implementation
   ivar block) without `@synthesize`, as this leaves the inherited property
   getter unimplemented (returning `nil`).

Conversely, in coordinators presenting modally or without a navigation stack, do
not synthesize `baseNavigationController` or clear it in `-stop`, avoiding
unused ivars and redundant teardown boilerplate.

**Why.** `ChromeCoordinator` declares
`@property(weak, nonatomic, readonly) UINavigationController* baseNavigationController;`
but does not synthesize a backing ivar or accessor in the base class. Pushed
coordinators require explicit subclass synthesis and assignment in
`-initWithBaseNavigationController:browser:` so that callers and child
coordinators can access `self.baseNavigationController`. Declaring a standalone
ivar without `@synthesize` duplicates state and leaves the getter returning
`nil`, while downcasting `baseViewController` is verbose and error-prone.
Conversely, modally presented coordinators do not use navigation presentation;
synthesizing `baseNavigationController` introduces unused backing state and dead
cleanup boilerplate in `-stop`.

## Examples

Bad:

```cpp
// Bad: Declaring a separate ivar leaves self.baseNavigationController nil; downcasting baseViewController
@implementation PushedCoordinator {
  FooViewController* _viewController;
  UINavigationController* _baseNavigationController;
}
- (void)start {
  _viewController = [[FooViewController alloc] init];
  UINavigationController* nav =
      base::apple::ObjCCastStrict<UINavigationController>(self.baseViewController);
  [nav pushViewController:_viewController animated:YES];
}
@end

// Bad: Modal coordinator synthesizing unused navigation controller and clearing in -stop
@implementation ModalCoordinator {
  FooViewController* _viewController;
}
@synthesize baseNavigationController = _baseNavigationController;
- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
  _baseNavigationController = nil;
}
@end
```

Good:

```cpp
// Good: Synthesize baseNavigationController, assign in designated init, and push directly
@implementation PushedCoordinator {
  FooViewController* _viewController;
}
@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:(UINavigationController*)navController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navController browser:browser];
  if (self) {
    _baseNavigationController = navController;
  }
  return self;
}
- (void)start {
  _viewController = [[FooViewController alloc] init];
  [self.baseNavigationController pushViewController:_viewController animated:YES];
}
@end

// Good: Modal coordinator omits unused baseNavigationController synthesis and
// teardown; -stop still dismisses the presented view controller deterministically.
@implementation ModalCoordinator {
  FooViewController* _viewController;
}
- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
}
@end
```
