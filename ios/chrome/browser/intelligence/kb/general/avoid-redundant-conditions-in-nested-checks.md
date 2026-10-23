---
name: ios/avoid-redundant-conditions-in-nested-checks
description: When writing nested conditionals, else-if chains, or intermediate booleans, omit checks already guaranteed, ruled out, or contradictory; factor logic into distinct primitive checks
rubric: readability
severity: should_fix
---

**Rule.** Do not repeat condition checks, boolean terms, or ternary expressions
(`? :`) inside a scope or block already guarded by an equivalent condition that
establishes their outcome; directly use the guaranteed branch value or logic and
eliminate unreachable fallback branches. Do not guard code with nested
conditions that contradict enclosing branch conditions or outer guards (such as
checking `!Flag()` in an outer guard and branching on `Flag()` within), ensuring
all nested conditional predicates are reachable and consistent with enclosing
branches to prevent impossible logic states. Additionally, do not define
intermediate boolean variables or composite conditions whose sub-conditions
overlap with, duplicate, or contradict the conditional expressions where they
are consumed. In `if ... else if` ladders, test the specific condition for each
branch directly rather than evaluating a composite condition or pre-aggregated
flag whose earlier terms were already eliminated by preceding branches; factor
complex branching decisions into distinct primitive boolean checks and clean,
mutually exclusive branches.

**Why.** Re-testing conditions whose outcome is already established by an
enclosing guard or ruled out as false in an else block introduces dead code,
duplicates evaluation, and adds visual clutter. Conversely, writing nested
conditions that contradict enclosing guards (such as requiring a predicate to be
true when an enclosing branch requires it to be false) creates impossible
conditions and unreachable dead code that silently prevents intended behavior
from executing. Furthermore, combining overlapping or contradictory conditions
into intermediate booleans or testing already-eliminated terms in `else if`
ladders masks each branch's actual entry criteria, obscures developer intent,
and creates fragile logic prone to regression during refactoring.

## Examples

Bad:

```cpp
// Contradictory nested condition ruled out by outer guard:
if (IsChromeNextIaEnabled() && !IsFullscreenRefactoringEnabled()) {
  if (IsFullscreenRefactoringEnabled()) {
    UpdateFullscreenLayout();
  }
}

// Intermediate boolean overlaps with check or precedes else-if ladder:
bool is_scan = HasScanSuggestion(items);
bool did_trigger = is_scan || HasCardSuggestion(items);
if (is_scan) {
  ShowScanSheet();
} else if (did_trigger) {
  ShowPaymentSheet();
}

// Redundant check and ternary guaranteed by enclosing guard:
if (CanShowTabStrip(self)) {
  CGFloat height = CanShowTabStrip(self) ? kToolbarHeight : kCompactHeight;
  if (CanShowTabStrip(self) && is_valid) {
    ApplyLayout(height);
  }
}
```

Good:

```cpp
// Allow inner flag to execute by removing contradictory outer requirement:
if (IsChromeNextIaEnabled()) {
  if (IsFullscreenRefactoringEnabled()) {
    UpdateFullscreenLayout();
  }
}

// Factor into distinct primitive checks for mutually exclusive branches:
bool is_scan = HasScanSuggestion(items);
bool has_cards = HasCardSuggestion(items);
if (is_scan) {
  ShowScanSheet();
} else if (has_cards) {
  ShowPaymentSheet();
}

// Use guaranteed value directly and omit redundant guard terms:
if (CanShowTabStrip(self)) {
  CGFloat height = kToolbarHeight;
  if (is_valid) {
    ApplyLayout(height);
  }
}
```
