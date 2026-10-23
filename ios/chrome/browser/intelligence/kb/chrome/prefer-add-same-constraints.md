---
name: ios/prefer-add-same-constraints
description: When pinning, matching, or centering views or layout guides in iOS Chrome, use constraints_ui_util helpers like AddSameConstraints instead of manual NSLayoutConstraint arrays
rubric: readability
severity: should_fix
---

**Rule.** When pinning, matching, or centering a view or layout guide against
another view, superview, or layout guide in iOS Chrome UI code (with or without
insets), use the Auto Layout helpers from
`ios/chrome/common/ui/util/constraints_ui_util.h`—such as `AddSameConstraints`,
`AddSameConstraintsWithInsets`, `AddSameConstraintsWithInset`,
`AddSameConstraintsToSides`, `AddSameConstraintsToSidesWithInsets` (using
`LayoutSides` bitmasks), `AddSameCenterConstraints`, or `PinToSafeArea`. Avoid
manually creating and activating arrays of individual `topAnchor`,
`bottomAnchor`, `leadingAnchor`, or `trailingAnchor` `NSLayoutConstraint`
anchors. Similarly, do not manually create and activate combined center
(`centerXAnchor`, `centerYAnchor`) and dimension (`widthAnchor`, `heightAnchor`)
constraints when binding a view to match another's bounds; use
`AddSameConstraints` (or `AddSameCenterConstraints` when only matching centers).

**Why.** Manually creating individual edge, center, or dimension constraints
introduces repetitive boilerplate and increases the risk of subtle mistakes,
such as sign inversion errors when applying bottom and trailing margin insets,
mismatched directional anchors, or omitted edges. The `constraints_ui_util.h`
helper family encapsulates binding matching anchors in a concise, readable call
that clearly communicates layout intent, handles directional insets safely, and
works across both `UIView` and `UILayoutGuide`.

## Examples

Bad:

```cpp
// 1. Manually activating individual edge constraints:
[NSLayoutConstraint activateConstraints:@[
  [subview.topAnchor constraintEqualToAnchor:parent.topAnchor constant:kTopInset],
  [subview.leadingAnchor constraintEqualToAnchor:parent.leadingAnchor],
  [subview.trailingAnchor constraintEqualToAnchor:parent.trailingAnchor],
  [subview.bottomAnchor constraintEqualToAnchor:parent.bottomAnchor],
]];

// 2. Matching bounds via manual center and dimension anchors:
[NSLayoutConstraint activateConstraints:@[
  [animView.centerXAnchor constraintEqualToAnchor:avatarView.centerXAnchor],
  [animView.centerYAnchor constraintEqualToAnchor:avatarView.centerYAnchor],
  [animView.widthAnchor constraintEqualToAnchor:avatarView.widthAnchor],
  [animView.heightAnchor constraintEqualToAnchor:avatarView.heightAnchor],
]];

// 3. Pinning edges with insets manually (prone to sign errors on trailing/bottom):
[NSLayoutConstraint activateConstraints:@[
  [view.topAnchor constraintEqualToAnchor:guide.topAnchor],
  [view.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:insets.leading],
  [view.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-insets.trailing],
  [view.bottomAnchor constraintEqualToAnchor:guide.bottomAnchor constant:-insets.bottom],
]];
```

Good:

```cpp
// 1. Pinning edges with insets using constraints_ui_util:
NSDirectionalEdgeInsets insets = NSDirectionalEdgeInsetsMake(kTopInset, 0, 0, 0);
AddSameConstraintsWithInsets(subview, parent, insets);

// 2. Matching bounds (replaces manual center + dimension anchors):
AddSameConstraints(avatarView, animView);

// 3. Pinning specific sides with insets:
AddSameConstraintsToSidesWithInsets(
    view, guide,
    LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom,
    insets);
```
