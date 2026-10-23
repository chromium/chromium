---
name: ios/guard-ui-updates-with-change-checks
description: In Objective-C UI update methods, property setters, and consumer methods, return early when state is unchanged before mutating ivars or triggering UI side effects
rubric: readability
severity: should_fix
---

**Rule.** In custom Objective-C property setters, consumer methods, and UI
update methods that perform side effects (such as updating subviews,
recalculating layouts, restarting animations, or notifying delegates and
observers), verify whether the state or property has actually changed and return
early before updating state or reconfiguring UI. In property setters and
consumer update methods, compare the incoming argument against the backing
instance variable (`if (value == _ivar) { return; }` for scalars/pointers, or
using `isEqual:`/`isEqualToString:` with nil-safe checks for object types)
before reassigning the ivar and updating subviews, buttons, or menus. In UI
update methods (such as visibility updates, layout adjustments, or mode
transitions), check whether view properties (such as `.hidden`, `.alpha`, or
`.transform`) or layout state already match the target values before mutating
views, scheduling Auto Layout passes, launching UIKit animations
(`[UIView animateWithDuration:...]`), or firing delegate callbacks.

**Why.** Mutating view properties, reconstructing subview hierarchies, launching
UIKit animations, or firing delegate and observer callbacks when state is
unchanged forces redundant Auto Layout passes, unnecessary CPU cycles and
allocations, and visual churn (such as restarting in-flight animations or
resetting scroll positions). Returning early when incoming property values or
visual states match existing state prevents dropped frames, visual glitches, and
redundant downstream layout work.

## Examples

Bad:

```cpp
// Setter updating backing state and subviews without an equality check:
- (void)setTitle:(NSString*)title {
  _title = [title copy];
  [self updateTitleLabel];
}

// UI update method executing animations and callbacks unconditionally:
- (void)updateToolbarVisibility:(BOOL)hideToolbar {
  self.view.hidden = hideToolbar;
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kDuration
                   animations:^{
                     [weakSelf showToolbarContainer];
                   }
                   completion:^(BOOL finished) {
                     [weakSelf notifyToolbarHeightDidChange];
                   }];
}
```

Good:

```cpp
// Return early when setter value matches current state:
- (void)setTitle:(NSString*)title {
  if (_title == title || [_title isEqualToString:title]) {
    return;
  }
  _title = [title copy];
  [self updateTitleLabel];
}

// Return early when view properties already match the target state:
- (void)updateToolbarVisibility:(BOOL)hideToolbar {
  if (self.view.hidden == hideToolbar && _container.alpha == 1.0) {
    return;
  }
  self.view.hidden = hideToolbar;
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kDuration
                   animations:^{
                     [weakSelf showToolbarContainer];
                   }
                   completion:^(BOOL finished) {
                     [weakSelf notifyToolbarHeightDidChange];
                   }];
}

- (void)showToolbarContainer {
  _container.alpha = 1.0;
}

- (void)notifyToolbarHeightDidChange {
  [self.delegate toolbarViewControllerDidChangeHeight:self];
}
```
