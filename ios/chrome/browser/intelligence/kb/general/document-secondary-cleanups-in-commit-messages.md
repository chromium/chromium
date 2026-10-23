---
name: ios/document-secondary-cleanups-in-commit-messages
description: When a CL includes secondary cleanups or adjustments, or defers related type cleanups to follow-up CLs, document those modifications or scope boundaries in the commit description
rubric: readability
severity: should_fix
citations:
  - path: docs/contributing.md
    heading: Uploading a change for review
    quote: Uploading a change for review
  - path: docs/cl_tips.md
    heading: Share context for the CL
    quote: Your CL description should always document what you are changing and why.
---

**Rule.** When a CL includes secondary cleanups, redundant code deletions, or
supporting adjustments (such as updating a flag, timeout, or visual styling
parameters) alongside its primary change, explicitly document and justify those
incidental modifications in the commit description body. Conversely, when
scoping a feature removal or dead-code cleanup to flag-gated logic while
deferring shared types, properties, or preferences to follow-up CLs to keep
changes small, or when cleanups accompany additions and relocations, explicitly
document the deferred cleanup boundary or accompanying changes in the
description.

**Why.** Reviewers, triage bots, and engineers investigating regressions or
bisecting history rely on the CL description to understand every intentional
modification and change boundary. Omitting incidental cleanups leaves
maintainers unsure whether deletions or styling tweaks were intentional, while
failing to document intentionally deferred cleanups causes reviewers to question
omitted types and risks scope creep.

## Examples

Bad:

```cpp
// Variant 1: Secondary cleanups omitted alongside feature
[iOS][AppBar] Configure Tab Grid Button
This change adds a long press context menu for the Tab Grid button.

// Variant 2: Secondary additions omitted alongside large deletions
[iOS][follow-deprecation] Remove follow model objects
This CL removes unused follow model objects, including browser agent.

// Variant 3: Supporting flag or timeout adjustment omitted alongside fix
[iOS] Fix Flaky testShowTabGroupInGridOnStart
This CL disables testShowTabGroupInGridOnStart on iPad due to Stage Manager.

// Variant 4: Visual styling adjustment omitted alongside fix
[iOS] Fix background issue on topToolbar buttons iOS 17-18
This CL fixes button styling for menu and close buttons.

// Variant 5: Deferred cleanup scope omitted in feature-removal CL
Remove arm 4 from ShopCard (price trackable item on tab resumption)
This arm was not a launch candidate.
```

Good:

```cpp
// Variant 1: Secondary cleanups documented
[iOS][AppBar] Configure Tab Grid Button
This change adds a long press context menu for the Tab Grid button. Some
redundant calls to consumer updates are also removed.

// Variant 2: Accompanying additions/deprecations documented
[iOS][follow-deprecation] Remove follow model objects
This CL removes unused follow model objects and deprecates profile prefs used.

// Variant 3: Supporting flag or timeout adjustment documented
[iOS] Fix Flaky testShowTabGroupInGridOnStart
This CL disables testShowTabGroupInGridOnStart on iPad due to Stage Manager. It
also updates ReturnToHomeSurfaceDuration to prevent timing flakes.

// Variant 4: Visual styling adjustment documented with design rationale
[iOS] Fix background issue on topToolbar buttons iOS 17-18
This CL fixes button styling for menu and close buttons. Additionally,
the alpha is updated from 0.6 to 0.2 to match the Figma specification.

// Variant 5: Deferred cleanup scope documented to clarify boundaries
Remove arm 4 from ShopCard (price trackable item on tab resumption)
This arm was not a launch candidate. This CL focuses on the feature flag
gated code. The type kPriceTrackableProductOnTab and preference
kTabResumptionWithPriceTrackableUrlImpressions will be removed in
subsequent CLs to keep changes small and focused.
```
