---
name: ios/capture-weak-delegate-in-completion-blocks
description: When executing commands or notifying delegates in coordinator dismissal or completion blocks, capture the command handler or weak delegate beforehand instead of weakSelf
rubric: safety
severity: should_fix
---

**Rule.** When invoking a delegate callback or executing a command inside an
asynchronous completion or animation block (such as view controller dismissals,
presentations, or coordinator `-stop` flows), resolve and capture the target
receiver directly before dispatching the block rather than dereferencing
`weakSelf` or capturing `self` strongly. Specifically:

1. For delegate callbacks, capture a direct weak reference beforehand (e.g.,
   `__weak id<FooDelegate> delegate = self.delegate;`) and invoke the callback
   directly on `delegate` rather than messaging `weakSelf.delegate`.
2. When executing commands after dismissing UI, capture the command handler in a
   local variable before the dismissal call (e.g.,
   `id<FooCommands> fooHandler = self.fooHandler;` or
   `HandlerForProtocol(self.browser->GetCommandDispatcher(), FooCommands)`)
   rather than dereferencing `self` or `weakSelf` inside the block.

**Why.** Dismissing a coordinator or view controller's UI often triggers parent
coordinators to stop and deallocate `self` before the asynchronous animation or
dismissal completion block fires. In Objective-C, messaging `nil` is a safe
runtime no-op; consequently, if `self` deallocates, `weakSelf` is zeroed by ARC,
and dereferencing `weakSelf.delegate` or `weakSelf.fooHandler` silently
evaluates to `nil`, dropping the delegate notification or command without
executing. Conversely, capturing `self` strongly keeps the entire coordinator
and view hierarchy alive longer than necessary, risking retain cycles. Capturing
the delegate weakly (`__weak id<FooDelegate> delegate = self.delegate;`) or
capturing the command handler locally beforehand decouples the callback from
`self`'s lifecycle and expresses the minimal required dependency, guaranteeing
reliable execution upon completion.

## Examples

Bad:

```cpp
// Bad Variant 1: Dereferencing weakSelf.delegate drops notification if self deallocates during dismissal.
- (void)stop {
  __weak __typeof(self) weakSelf = self;
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf.delegate fooCoordinatorDidDismiss:weakSelf];
                         }];
  _viewController = nil;
}

// Bad Variant 2: Dereferencing weakSelf.settingsHandler drops command upon teardown.
__weak __typeof(self) weakSelf = self;
[self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
  [weakSelf.settingsHandler showDefaultSearchEngineSettings];
}];
```

Good:

```cpp
// Good Variant 1: Capture weak delegate directly before dismissal.
- (void)stop {
  __weak id<FooCoordinatorDelegate> delegate = self.delegate;
  __weak __typeof(self) weakSelf = self;
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [delegate fooCoordinatorDidDismiss:weakSelf];
                         }];
  _viewController = nil;
}

// Good Variant 2: Capture command handler before dismissal tears down self.
id<SettingsCommands> settingsHandler = self.settingsHandler;
[self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
  [settingsHandler showDefaultSearchEngineSettings];
}];
```
