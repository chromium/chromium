---
name: ios/combine-consecutive-early-return-guards
description: Combine consecutive early-return guards sharing identical bailout values using logical OR (||), ordering cheaper checks first, but keep prerequisite checks separate when setting error flags
rubric: readability
severity: nit
---

**Rule.** Combine consecutive independent early-return guards—such as null
checks, pointer validity checks, or related boolean capability and status
flags—into a single compound if condition using logical OR (||) when they share
identical exit handling or bailout values (such as return;, return NO;, return
false;, return nil;, return nullptr;, or return 0;). Order cheaper, local, or
non-allocating predicates before more expensive checks such as function calls,
feature flag evaluations, or lookups. However, do not combine guards or set
specialized error or status flags when early-exiting for missing prerequisites
or null dependencies; keep prerequisite checks separate from domain validation
so specific failure flags are only assigned when domain validation itself fails.

**Why.** Consolidating consecutive independent guard checks into a compound ||
condition eliminates repetitive branching boilerplate, reduces visual clutter,
and clarifies control flow. Ordering cheap local checks before function calls or
feature flag evaluations leverages short-circuit evaluation to skip expensive or
secondary checks. However, setting domain-specific error flags on missing
prerequisites misattributes errors, creates fragile dependencies on evaluation
order, and misleads downstream error reporting.

## Examples

Bad:

```cpp
// Bad: Consecutive separate guard blocks calling feature flag before local check.
if (IsPasswordRemovalFromDeleteBrowsingDataEnabled()) {
  return nil;
}
if (!_shouldShowFooter) {
  return nil;
}

// Bad: Redundant separate guard blocks with identical early returns.
if (_galleryActionsDisabled) {
  return NO;
}
if (_galleryActionsHidden) {
  return NO;
}

// Bad: Combining prerequisite check with domain check sets specialized flag on missing dependency.
if (!_webState || !CanExtractPageContextForWebState(_webState.get())) {
  _notExtractable = YES;
  [self asyncWorkCompletedForPageContext];
  return;
}
```

Good:

```cpp
// Good: Combine guards with ||, placing cheaper local check before feature flag evaluation.
if (!_shouldShowFooter ||
    IsPasswordRemovalFromDeleteBrowsingDataEnabled()) {
  return nil;
}

// Good: Combine independent guards sharing identical return value into compound condition.
if (_galleryActionsDisabled || _galleryActionsHidden) {
  return NO;
}

// Good: Keep prerequisite check separate from domain validation setting specialized status flag.
if (!_webState) {
  [self asyncWorkCompletedForPageContext];
  return;
}
if (!CanExtractPageContextForWebState(_webState.get())) {
  _notExtractable = YES;
  [self asyncWorkCompletedForPageContext];
  return;
}
```
