---
name: ios/coordinator-teardown-disconnects-mediator-in-stop
description: In coordinator -stop methods, deterministically stop child coordinators, unregister command targets, disconnect mediators, and dismiss views
rubric: safety
severity: blocking
citations:
  - path: ios/architecture/coordinators.md
    heading: Deterministic Teardown in `-stop`
    quote: "Every coordinator's `-stop` implementation must deterministically tear down the feature: 1. **Stop child coordinators**: Call `-stop` on any active child coordinators and set their instance variables to `nil`. 2. **Stop command dispatching**: Unregister any command targets registered by the coordinator on the `CommandDispatcher` (via `-stopDispatchingToTarget:`). 3. **Disconnect mediators**: Call `-disconnect` on any mediators owned by the coordinator so they immediately end observation of C++ model objects (`WebStateList`, keyed services, `PrefService`), then set their instance variables to `nil`. 4. **Dismiss or remove the view controller**: Dismiss or detach the managed view controller and set its instance variable to `nil`."
  - path: ios/best_practices.md
    heading: Deallocation and Deterministic Cleanup
    quote: 'Objective-C object deallocation timing (`-dealloc`) is non-deterministic: UIKit animations, presentation controllers, blocks, and autorelease pools often keep objects alive after a feature is dismissed. **Never rely on `-dealloc` for cleanup required for correctness** (such as removing C++ observers or stopping command dispatch).'
---

**Rule.** In a coordinator's `-stop`, deterministically tear down all owned
child components before releasing references, never relying on `-dealloc`:

1. Stop child coordinators: call `-stop` on every active child coordinator and
   set its instance variable to `nil`.
2. Stop command dispatching: unregister any command targets the coordinator
   registered on the `CommandDispatcher` (`-stopDispatchingToTarget:`).
3. Disconnect mediators: call `-disconnect` on every owned mediator so
   observation of C++ model objects ends immediately, then set its instance
   variable to `nil`. Never simply reset references to `nil` without invoking
   `-stop` or `-disconnect`.
4. Dismiss or remove the view controller (for modal presentations,
   `[_viewController.presentingViewController dismissViewControllerAnimated:completion:]`,
   which is a safe no-op if UIKit already dismissed it) and set its instance
   variable to `nil`.

**Why.** Objective-C deallocation timing (`-dealloc`) is non-deterministic
because UIKit animations, presentation controllers, blocks, and autorelease
pools can keep objects alive after dismissal. Omitting `-stop`,
`-stopDispatchingToTarget:`, or `-disconnect` can leave command targets
registered and C++ model observers attached after their services are destroyed,
risking crashes or `CHECK` failures. See
`ios/architecture/coordinators.md#deterministic-teardown-in-stop` and
`ios/best_practices.md#deallocation-and-deterministic-cleanup`.

## Examples

Bad:

```cpp
// Resetting references to nil without stopping child coordinator or
// disconnecting mediator:
- (void)stop {
  [super stop];
  _creditCardScannerCoordinator = nil;
  _mediator = nil;
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _viewController = nil;
}
```

Good:

```cpp
// Stop child coordinator, command dispatch, mediator, and view controller:
- (void)stop {
  [super stop];
  [_creditCardScannerCoordinator stop];
  _creditCardScannerCoordinator = nil;
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [_mediator disconnect];
  _mediator = nil;
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _viewController = nil;
}
```
