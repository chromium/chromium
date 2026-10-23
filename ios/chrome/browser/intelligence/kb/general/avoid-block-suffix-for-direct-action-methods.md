---
name: ios/avoid-block-suffix-for-direct-action-methods
description: Name Objective-C direct actions with imperative verbs and callbacks with past-tense, on<Event>:, or handle<Action>Result:, avoiding closure tokens and unused forwarded parameters
rubric: readability
severity: should_fix
---

**Rule.** Do not name Objective-C direct action methods with closure tokens or
invocation timing, nor name completion callbacks with imperative action verbs:

1. **Direct action and cleanup methods**: Name methods with imperative active
   verb phrases describing the concrete operation performed (e.g.,
   `moveTabToGroup:`, `stopManagedConfirmation`, or
   `setHeightConstraintToConstant:`). Avoid prefixes like `blockFor...` or
   `callbackFor...`, or suffixes like `Block` or `Done`, even if the method
   executes within an animation block or callback context.
2. **Completion callbacks and post-action handlers**: Name callback and
   completion handlers using standard Objective-C `did`/`will` event phrasing
   (e.g., `didDismissViewControllerWithCompletion:`,
   `didCompletePresentationAnimation`, or `didFinish...`) or descriptive result
   handlers (e.g., `handle<Action>Result:` such as
   `handleSignInResult:identity:`). Prefer `did*`/`will*` over noun-participle
   names such as `viewControllerDismissedWithCompletion:`. Do not use imperative
   action verbs (such as `dismissViewControllerWithCompletion:` or
   `updateSnapshot:`), noun phrases ending in `Completion` (e.g.,
   `presentationAnimationCompletion`), or filler phrasing (e.g.,
   `do...CompletionWithResult:`). Reserve imperative verb phrases for methods
   that initiate or execute operations.
3. **Parameter omission**: When implementing helper methods invoked as
   completion blocks, omit unused parameters forwarded from the underlying
   asynchronous callback.

**Why.** In Objective-C, `block` denotes a closure type (`^()`), whereas
imperative verb phrases indicate that a method initiates an action. Naming
regular member methods with `blockFor...` or `callbackFor...`, or appending
suffixes like `Block`, `Callback`, or `Done`, misleads callers into expecting a
block-returning factory function, a closure property, or a notification token
rather than an active member method. Conversely, using imperative action verbs
for post-action callbacks or asynchronous completion handlers confuses call
direction and execution sequence, misleading callers into thinking the method
initiates the operation rather than handling its completion. Similarly, naming
callback handlers after invocation timing, filler patterns (e.g.,
`do...CompletionWithResult:`), or noun phrases ending in `Completion` obscures
the actual operation performed and resembles property accessors. Using
imperative verbs for direct actions and `did`/`will` event phrasing
(`didComplete...`, `didDismiss...`) or descriptive result handlers adheres to
Cocoa conventions and clarifies execution flow. Additionally, omitting unused
parameters forwarded from completion blocks keeps method signatures clean and
avoids obscuring true dependencies.

## Examples

Bad:

```cpp
// Direct action and cleanup methods suffixed or prefixed with closure tokens:
- (void)moveTabToGroupBlock:(const TabGroup*)group {
  self.webStateList->MoveToGroup({tabIndex}, group);
  [self dismissMenu];
}

- (void)blockForAnimatingHeightConstraintToConstant:(CGFloat)constant {
  _heightConstraint.constant = constant;
  [self.view.superview layoutIfNeeded];
}

// Post-action callback or completion handler named with imperative action verbs:
- (void)dismissViewControllerWithCompletion:(void (^)())completion {
  self.dismissingModal = NO;
  if (completion) {
    completion();
  }
}

// Completion handler using noun completion suffix or imperative verb with unused parameters:
- (void)presentationAnimationCompletion {
  [_delegate assistantContainerDidPresent];
}

snapshotTabHelper->UpdateSnapshotWithCallback(^(UIImage* image) {
  [weakSelf updateSnapshot:image withCallback:callback];
});
- (void)updateSnapshot:(UIImage*)image withCallback:(ProceduralBlock)callback {
  _snapshotTimer.Stop();
  [self runSnapshotCallback:callback];
}
```

Good:

```cpp
// Direct action and cleanup methods use active imperative verb phrases:
- (void)moveTabToGroup:(const TabGroup*)group {
  self.webStateList->MoveToGroup({tabIndex}, group);
  [self dismissMenu];
}

- (void)setHeightConstraintToConstant:(CGFloat)constant {
  _heightConstraint.constant = constant;
  [self.view.superview layoutIfNeeded];
}

// Post-action callback named with did* event phrasing:
- (void)didDismissViewControllerWithCompletion:(void (^)())completion {
  self.dismissingModal = NO;
  if (completion) {
    completion();
  }
}

// Callbacks named with did* phrasing or descriptive result handlers:
- (void)didCompletePresentationAnimation {
  [_delegate assistantContainer:container didAppearAnimated:YES];
}

- (void)handleSignInResult:(SigninCoordinatorResult)result
                  identity:(id<SystemIdentity>)identity {
  if (result == SigninCoordinatorResultSuccess) {
    [self proceedWithIdentity:identity];
  }
}

// Async callback uses did* phrasing and drops the unused parameter:
snapshotTabHelper->UpdateSnapshotWithCallback(^(UIImage* image) {
  [weakSelf didUpdateSnapshotWithCallback:callback];
});
- (void)didUpdateSnapshotWithCallback:(ProceduralBlock)callback {
  _snapshotTimer.Stop();
  [self runSnapshotCallback:callback];
}
```
