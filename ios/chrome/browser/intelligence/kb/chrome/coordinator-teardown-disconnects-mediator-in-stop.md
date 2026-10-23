---
name: ios/coordinator-teardown-disconnects-mediator-in-stop
description: In coordinator -stop methods, disconnect mediators, clear delegates and consumers before niling ivars, stop child coordinators in independent if blocks, and reset blocks
rubric: safety
severity: blocking
citations:
  - path: ios/architecture/coordinators.md
    heading: Deterministic Teardown in `-stop`
    quote: "Every coordinator's `-stop` implementation must deterministically tear down the feature: 1. **Stop child coordinators**: Call `-stop` on any active child coordinators and set their instance variables to `nil`. 2. **Disconnect the mediator**: Call `[_mediator disconnect]` and set `_mediator = nil`."
  - path: ios/best_practices.md
    heading: Deallocation and Deterministic Cleanup
    quote: 'Objective-C object deallocation timing (`-dealloc`) is non-deterministic: UIKit animations, presentation controllers, blocks, and autorelease pools often keep objects alive after a feature is dismissed. Never rely on `-dealloc` for cleanup required for correctness (such as removing C++ observers or stopping command dispatch).'
---

**Rule.** In a coordinator's `-stop`, deterministically tear down all owned
child components, dependencies, and helper state before releasing references,
never relying on `-dealloc`:

1. Stop child coordinators: call `[-stop]` on every active child coordinator and
   set its instance variable to `nil`. Tear down each active child coordinator
   and view controller in independent statements or `if` blocks rather than
   mutually exclusive `else if` branches, which skip teardown for subsequent
   active components.
2. Stop command dispatching: unregister any command targets the coordinator
   registered on the `CommandDispatcher` (`-stopDispatchingToTarget:`).
3. Disconnect mediators: call `[-disconnect]` on every owned mediator and state
   manager so observation of C++ model objects ends immediately, clear the
   properties that connect it to other objects (e.g.,
   `_mediator.consumer = nil;`), and only then set the instance variable to
   `nil`. Never assign a pointer to `nil` before completing cleanup calls on
   that object, as messaging or setting properties on `nil` in Objective-C is a
   silent no-op; never simply reset references to `nil` without invoking `-stop`
   or `-disconnect`.
4. Dismiss or remove the view controller (for modal presentations,
   `[_viewController.presentingViewController dismissViewControllerAnimated:completion:]`,
   which is a safe no-op if UIKit already dismissed it) and set its instance
   variable to `nil`.
5. Reset all remaining retained instance variables, helper state objects,
   injected C++ service pointers (to `nullptr`), and stored completion/callback
   blocks to prevent retain cycles and memory leaks.
6. Perform mediator disconnection and view controller dismissal in `-stop`
   rather than prematurely inside UI action handlers or custom dismissal helpers
   before notifying the parent or delegate.

**Why.** Objective-C object deallocation timing is non-deterministic: UIKit
animations, presentation controllers, blocks, and autorelease pools often keep
coordinators, mediators, and view controllers alive after a feature is
dismissed. If a coordinator omits calling `[-stop]` on active child coordinators
or `[-disconnect]` on owned mediators and state managers, active observer
registrations (e.g., on `Browser` or `WebStateList`) and raw service pointers
remain alive, causing use-after-free crashes, memory leaks, or CHECK failures.
In Objective-C, messaging or setting properties on `nil` is a silent no-op;
setting an instance variable to `nil` before clearing its consumer or delegate
properties leaves consumers or observers attached if the object is retained
elsewhere. Under ARC, blocks retain all referenced objects in their capture
scope, and failing to clear instance variables holding child coordinators,
mediators, view controllers, helper state, or stored completion blocks causes
retain cycles. Chaining cleanup with `else if` blocks falsely couples distinct
lifecycles, skipping teardown of subsequent active components if an earlier
branch matches. Finally, tearing down components in UI action handlers before
notifying the parent leads to premature destruction, whereas clearing the
mediator's consumer before niling guarantees no pending model updates reach
dismissed UI. See `ios/architecture/coordinators.md#deterministic-teardown-in`
and `ios/best_practices.md#deallocation-and-deterministic-cleanup`.

## Examples

Bad:

```cpp
// Variant 1: Chaining teardown in else-if skips active components, and omits mediator disconnect:
- (void)stop {
  [super stop];
  if (_viewController) {
    [_viewController dismissViewControllerAnimated:YES completion:nil];
    _viewController = nil;
  } else if (_scannedCardViewController) {
    [_creditCardScannerCoordinator stop];
    _creditCardScannerCoordinator = nil;
  }
  _mediator = nil;  // Missing [_mediator disconnect] and consumer cleanup!
}

// Variant 2: Niling ivar before clearing its consumer renders the cleanup a silent no-op:
- (void)stop {
  [super stop];
  [_mediator disconnect];
  _mediator = nil;
  _mediator.consumer = nil;
}

// Variant 3: Premature teardown in action handler before notifying the parent:
- (void)didTapPrimaryActionButton {
  [_mediator disconnect];
  _mediator = nil;
  [self.delegate managedProfileCreationCoordinator:self result:_mode];
}
```

Good:

```cpp
// Variant 1: Independent if blocks for each component, then dispatch, mediator and view controller teardown:
- (void)stop {
  [super stop];
  if (_creditCardScannerCoordinator) {
    [_creditCardScannerCoordinator stop];
    _creditCardScannerCoordinator = nil;
  }
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator = nil;
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _viewController = nil;
}

// Variant 2: Complete disconnect and clear the consumer before niling the ivar:
- (void)stop {
  [super stop];
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator = nil;
}

// Variant 3: Notify the parent first; perform teardown deterministically in -stop:
- (void)didTapPrimaryActionButton {
  [self.delegate managedProfileCreationCoordinator:self result:_mode];
}
```
