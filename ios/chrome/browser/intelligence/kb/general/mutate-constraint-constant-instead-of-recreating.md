---
name: ios/mutate-constraint-constant-instead-of-recreating
description: When updating layout constraints where target views and anchors remain unchanged, mutate NSLayoutConstraint.constant in place instead of recreating constraint instances
rubric: readability
severity: should_fix
---

**Rule.** When updating layout constraints where target views and anchors remain
identical, update the existing `NSLayoutConstraint` `.constant` property in
place rather than deactivating, recreating, and reactivating constraint
instances. Store the constraint reference during setup and mutate its
`.constant` during dynamic layout passes or state updates.

**Why.** Recreating `NSLayoutConstraint` instances during layout updates causes
unnecessary object allocations and incurs Auto Layout engine churn by discarding
and rebuilding layout solver equations. Mutating the `.constant` property
updates the underlying layout equation in place efficiently without churn.

## Examples

Bad:

```cpp
- (void)updateBottomBarConstraints {
  _bottomBarBottomConstraint.active = NO;
  if (self.layoutState.appBarPosition == AppBarPosition::kBottom) {
    _bottomBarBottomConstraint = [_bottomBar.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-AppBarHeightPortrait()];
  } else {
    _bottomBarBottomConstraint = [_bottomBar.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor];
  }
  _bottomBarBottomConstraint.active = YES;
}
```

Good:

```cpp
- (void)updateBottomBarConstraints {
  _bottomBarBottomConstraint.constant =
      self.layoutState.appBarPosition == AppBarPosition::kBottom
          ? -AppBarHeightPortrait()
          : 0;
}
```
