---
name: ios/split-large-changes-or-bypass-presubmit-warning
description: When authoring CLs adding more than ~300 lines of production code, split them into smaller changes or document reviewer-approved bypass with BYPASS_LARGE_CHANGE_WARNING
rubric: design
severity: nit
citations:
  - path: docs/cl_tips.md
    heading: Keep changes under 500 LoC
    quote: Large changes take longer to review than smaller changes. Reviewers generally need to familiarize themselves with the content of a CL after each round of review, so the larger a CL is, the longer that process takes... If your CL is larger than that, seriously consider splitting it into smaller, reviewable units.
---

**Rule.** Keep CLs small and split changes adding more than ~300 lines of
production code (or ~500 lines total) into smaller, focused, and reviewable
units tagged with the same tracking bug. When splitting a feature across a chain
of stacked CLs, never submit dead code: start from the integration point in
existing code and build inward (for example, introducing the coordinator and
wiring it into its parent coordinator first, then adding the view controller and
mediator in follow-up CLs). If a large change cannot practically be split and
code reviewers explicitly approve landing it as a single CL, silence the Gerrit
large-change analyzer warning by adding `BYPASS_LARGE_CHANGE_WARNING: <reason>`
to the commit description footers.

**Why.** Large CLs increase reviewer cognitive load, prolong review cycles,
increase the risk of overlooked bugs, and complicate regression bisection and
rollbacks. Starting stacked CLs from the integration point ensures every landed
CL is exercised rather than dead code. When a monolithic change is unavoidable
and reviewers agree, documenting the rationale with the
`BYPASS_LARGE_CHANGE_WARNING` Gerrit commit-message footer (checked by the
Gerrit analyzer rather than `//ios/PRESUBMIT.py`) records reviewer consent in
Git history.

## Examples

Bad:

```cpp
// Monolithic change exceeding production code size threshold without splitting
// or documenting reviewer approval:
[DPTH] Add DataProtectionTabHelper for screenshot protection.

Implement DataProtectionTabHelper and wire up URL lookup checks and screenshot
protection observers.

Bug: 485586852
Change-Id: If6ca9270b44c7704b8e78a786f98b936ab24d87e
```

Good:

```cpp
// Option 1: Split into focused, stacked CLs starting from the integration point
// and building inward (never landing unreferenced dead code):
// CL 1: Wire the feature entry point into the parent coordinator/scene agent.
// CL 2: Implement the tab helper and observer wiring behind the entry point.

// Option 2: When splitting is infeasible and reviewers explicitly approve:
[DPTH] Add DataProtectionTabHelper for screenshot protection.

Implement DataProtectionTabHelper and wire up URL lookup checks and screenshot
protection observers.

BYPASS_LARGE_CHANGE_WARNING: Approved by reviewers due to tight coupling.
Bug: 485586852
Change-Id: If6ca9270b44c7704b8e78a786f98b936ab24d87e
```
