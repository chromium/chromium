---
name: ios/deactivate-constraints-before-reinstalling
description: When replacing or toggling Auto Layout constraints, reparenting controllers, or re-adding reused views, deactivate existing constraints or call removeFromSuperview before activating replacements
rubric: correctness
severity: should_fix
---

**Rule.** When configuring, updating, replacing, or toggling Auto Layout
constraints, always deactivate existing or conflicting constraints before
reassigning references or activating replacements (for example, via
`[NSLayoutConstraint deactivateConstraints:]` or setting `.active = NO`).
Reassigning an active `NSLayoutConstraint` property or instance variable does
not remove or deactivate the prior constraint in UIKit's Auto Layout engine;
explicitly deactivate it before overwriting the reference or creating a new
constraint. In repeatedly invoked setup or state-update methods, retain
references to active constraints to deactivate them before creating
replacements. When switching mutually exclusive layout states, deactivate active
constraints before activating alternatives. In view controller containment
overrides (`didMoveToParentViewController:`), unconditionally deactivate and nil
out parent-anchored constraints before checking `if (parent)`. Additionally,
when dynamically re-adding, re-parenting, or re-constraining a reused view (such
as via `AddSameConstraints` or `activateConstraints:`), ensure prior constraints
are cleared—such as by calling `[view removeFromSuperview]` or deactivating
previous constraints—before re-inserting the view and attaching new constraints.

**Why.** In UIKit Auto Layout, adding new constraints (such as via
`AddSameConstraints` or `activateConstraints:`) or reassigning a property to a
new `NSLayoutConstraint` does not remove or deactivate existing constraints from
the view or its superview hierarchy. Leaving prior constraints active alongside
newly installed or replacement constraints creates an unsatisfiable Auto Layout
system at runtime, causing UIKit to log layout engine warnings, arbitrarily
break constraints, and produce visual layout glitches. Calling
`[view removeFromSuperview]` strips all constraints associated with that view
within its superview hierarchy, preventing duplicate constraints from
accumulating when a view is reused. During view controller reparenting, failing
to clear parent-anchored constraints before checking `parent` leaves stale
constraints attached to the previous parent's view hierarchy.

## Examples

Bad:

```cpp
// Overwriting active constraint reference leaves old constraint in hierarchy:
self.bottomConstraint = [popup.bottomAnchor constraintEqualToAnchor:container.bottomAnchor];
self.bottomConstraint.active = YES;

// Re-adding reused view without clearing existing constraints:
[parentView addSubview:reusedView];
AddSameConstraints(reusedView, parentView);

// Activating replacement before deactivating conflicting constraint:
_collectionViewBottomConstraint.active = YES;
[NSLayoutConstraint deactivateConstraints:_attributionTextViewConstraints];

// Reparenting without deactivating parent constraints:
- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (parent) { [self layoutInParentView:parent.view]; }
}
```

Good:

```cpp
// Deactivate existing constraint before reassigning reference:
self.bottomConstraint.active = NO;
self.bottomConstraint = [popup.bottomAnchor constraintEqualToAnchor:container.bottomAnchor];
self.bottomConstraint.active = YES;

// Remove reused view from superview first to clear prior constraints:
[reusedView removeFromSuperview];
[parentView addSubview:reusedView];
AddSameConstraints(reusedView, parentView);

// Deactivate conflicting constraint before activating replacement:
[NSLayoutConstraint deactivateConstraints:_attributionTextViewConstraints];
_collectionViewBottomConstraint.active = YES;

// Deactivate and nil out parent constraints before checking new parent:
- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (_parentConstraints) {
    [NSLayoutConstraint deactivateConstraints:_parentConstraints];
    _parentConstraints = nil;
  }
  if (parent) { [self layoutInParentView:parent.view]; }
}
```
