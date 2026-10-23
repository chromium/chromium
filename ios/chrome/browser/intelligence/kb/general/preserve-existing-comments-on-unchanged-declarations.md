---
name: ios/preserve-existing-comments-on-unchanged-declarations
description: When refactoring, restructuring, or extracting logic and declarations, preserve explanatory comments on active logic and invariants, but update comments contradicting refactored semantics or lifecycles
rubric: readability
severity: should_fix
citations:
  - path: docs/documentation_best_practices.md
    heading: Inline comments
    quote: The primary purpose of inline comments is to provide information that the code itself cannot contain, such as why the code is there.
  - path: docs/documentation_best_practices.md
    heading: Delete dead documentation
    quote: First delete what you're certain is wrong, ignore what's unclear.
  - path: agents/prompts/common.minimal.md
    heading: Coding
    quote: 'Add code comments sparingly: Focus on *why* something is done, not *what* is done.'
---

**Rule.** Do not remove existing explanatory comments on active logic (such as
metrics logging via `base::RecordAction`), subtle conditions (such as
`base::TimeDelta::magnitude()`), non-obvious return conditions, control flow, or
unchanged declarations when refactoring, restructuring, simplifying, or
extracting/inlining helper methods; preserve or adapt them directly above the
relocated construct unless obsolete or inaccurate. Conversely, when refactoring
member types, nullable pointers to references (`raw_ref`), or ownership models,
update comments that contradict refactored semantics (such as claiming smart
pointers are not used), and document both creation and destruction points when
lifetimes are tied to explicit lifecycle methods (such as `Init()` and
`CleanUp()` or `Shutdown()`).

**Why.** Explanatory comments capture critical design context, ownership
semantics, domain invariants, non-obvious return conditions, edge-case defenses,
and telemetry rationale not evident from syntax alone. Inadvertently dropping
inline comments when restructuring, moving, or extracting logic strips
institutional knowledge and forces future maintainers to reverse-engineer why
specific branches or conditions exist. Conversely, retaining obsolete comments
that contradict refactored semantics or omitting non-standard lifecycle teardown
points misleads maintainers about active behavior and memory management.

## Examples

Bad:

```cpp
// Dropping comments on relocated active logic or subtle conditions.
if (displayedBadge.badgeType == kBadgeTypeOverflow) {
  base::RecordAction(base::UserMetricsAction(kBadgeShownUserAction));
}
return (base::Time::Now() - lastDisplayTime).magnitude() < kInterval;

// Dropping comments explaining control flow when restructuring.
if (scrollerInfo) {
  interactionInfo.scrollerInfo = scrollerInfo;
  return interactionInfo;
}

// Retaining comments contradicting a refactored non-null reference or lifecycle.
// May be null if the promo has no feature.
const raw_ref<const base::Feature> feature_;
// Plain pointer, not a smart pointer.
raw_ptr<feature_engagement::Tracker> tracker_;
```

Good:

```cpp
// Preserving comments on relocated active logic and subtle conditions.
if (displayedBadge.badgeType == kBadgeTypeOverflow) {
  // Log that the overflow badge is being shown.
  base::RecordAction(base::UserMetricsAction(kBadgeShownUserAction));
}
// Magnitude prevents clock jumps from suppressing the infobar forever.
return (base::Time::Now() - lastDisplayTime).magnitude() < kInterval;

// Preserving explanatory comments on control flow and return conditions.
if (scrollerInfo) {
  // Just return the scroller info when not in actionable mode.
  interactionInfo.scrollerInfo = scrollerInfo;
  return interactionInfo;
}

// Updating comments to match refactored semantics and explicit lifecycles.
// The promo's feature; never null, bound in the constructor.
const raw_ref<const base::Feature> feature_;
// The feature engagement tracker. Set in Init(), cleared in CleanUp().
raw_ptr<feature_engagement::Tracker> tracker_;
```
