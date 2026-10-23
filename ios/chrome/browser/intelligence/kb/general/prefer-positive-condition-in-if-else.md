---
name: ios/prefer-positive-condition-in-if-else
description: In two-branch if/else blocks or ternary expressions, prefer positive conditions over negated expressions by swapping branches; use early returns for guards.
rubric: readability
severity: nit
---

**Rule.** When writing two-branch conditionals where both execution paths are
handled—such as `if`/`else` statements or ternary expressions (`? :`)—prefer
testing the affirmative condition (`if (condition)` or `condition ? a : b`)
rather than leading with a negated condition (`if (!condition)` or
`!condition ? a : b`). Swap the `if` and `else` bodies or ternary expression
operands to evaluate the positive path first. Do not apply this branch swapping
to guard clauses or error bailouts; instead, when handling missing preconditions
or error checks, refactor them into early returns without an `else` block to
keep control flow flat.

**Why.** Evaluating affirmative conditions aligns with natural mental models and
reduces cognitive load by avoiding double negatives and inverted logic across
alternative branches. When reading two-branch constructs, readers intuitively
expect the first branch to represent the primary, active, or enabled state, and
the second branch to represent the alternative, fallback, or disabled state. For
guard conditions and error bailouts, refactoring inverted `if/else` checks into
early returns flattens control flow and eliminates unnecessary nesting.

## Examples

Bad:

```cpp
// Two-branch if/else: avoid testing negated condition when both branches are handled
if (!IsProactiveSuggestionsFrameworkEnabled()) {
  self.displayedBadge = nil;
} else {
  [self clearAllBadges];
}

// Ternary expression: avoid leading with negated condition
textColor = !enabled ? kDisabledColor : kEnabledColor;

// Guard clause: avoid branching with if/else instead of early return
if (!CanExtractPageContext(web_state)) {
  context.state = State::kBlocked;
} else {
  Process(web_state);
}
```

Good:

```cpp
// Two-branch if/else: test affirmative condition and swap branches
if (IsProactiveSuggestionsFrameworkEnabled()) {
  [self clearAllBadges];
} else {
  self.displayedBadge = nil;
}

// Ternary expression: test affirmative condition and swap operands
textColor = enabled ? kEnabledColor : kDisabledColor;

// Guard clause: return early to eliminate if/else nesting
if (!CanExtractPageContext(web_state)) {
  context.state = State::kBlocked;
  return;
}
Process(web_state);
```
