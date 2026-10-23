---
name: ios/consolidate-duplicate-conditionals-into-if-else
description: When multiple statements or fragmented checks evaluate the same condition or mode, consolidate into a single conditional block or delegate to mode-specific helpers
rubric: readability
severity: should_fix
---

**Rule.** Do not evaluate the same condition, feature flag, or operational mode
across multiple consecutive ternary expressions, fragmented checks (such as
consecutive `if (!condition)` and `if (condition) ... else` blocks), or repeated
identical `if` blocks within the same local scope. Consolidate related
statements, property initializations, and branch-specific setup into a single
unified `if` or `if`/`else` block when intermediate code has no side-effect
dependencies or ordering constraints. When branch-specific configurations or
container setups are extensive or distinct, branch once at the top level and
delegate to dedicated helper methods for each mode or variant.

**Why.** Evaluating the same condition or mode repeatedly across separate
statements, fragmented checks, repeated if blocks, or multi-line ternary
expressions fragments control flow, obscures mutually exclusive execution paths,
and risks logic divergence. Consolidating logic into a unified conditional block
or delegating to mode-specific helpers keeps related operations cohesive,
clarifies execution paths, and simplifies maintenance and feature flag cleanup.

## Examples

Bad:

```cpp
// Bad: Repeated identical checks separated by intermediate statements.
if (view == self.tabStripView) {
  self.topConstraint.constant = yOrigin;
}
if (behaviour != Overlap) {
  height += CGRectGetHeight(frame);
}
if (view == self.tabStripView) {
  [self setNeedsStatusBarAppearanceUpdate];
}

// Bad: Fragmented checks and consecutive ternaries on the same condition.
BOOL isRegularWidth =
    self.traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular;
if (!isRegularWidth) {
  stackView = [[UIStackView alloc] init];
}
if (isRegularWidth) {
  [stackView addArrangedSubview:label];
} else {
  [stackView addArrangedSubview:subview];
}

// Bad: Repeated branching on the same mode across property assignments.
_badgeIcon.tintColor = [self useBadge] ? kWhite : kDefault;
_container.backgroundColor = [self useBadge] ? kClear : kDefault;
```

Good:

```cpp
// Good: Consolidated into a single conditional block.
if (view == self.tabStripView) {
  self.topConstraint.constant = yOrigin;
  [self setNeedsStatusBarAppearanceUpdate];
}
if (behaviour != Overlap) {
  height += CGRectGetHeight(frame);
}

// Good: Consolidated into a unified if/else block (branching on the size
// class, never on the device model).
BOOL isRegularWidth =
    self.traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular;
if (isRegularWidth) {
  [stackView addArrangedSubview:label];
} else {
  stackView = [[UIStackView alloc] init];
  [stackView addArrangedSubview:subview];
}

// Good: Delegated to mode-specific helper methods.
if ([self useBadge]) {
  [self updateBadgeForUnifiedContainer];
} else {
  [self updateBadgeForLegacyContainer];
}
```
