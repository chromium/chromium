---
name: ios/gate-new-features-behind-feature-flag
description: When introducing or modifying features, UI components, migrations, or shared flows, gate behavior behind FeatureList::IsEnabled or Is*Enabled() helpers, preserving exact baseline functionality when disabled
rubric: correctness
severity: should_fix
citations:
  - path: docs/flag_guarding_guidelines.md
    heading: When is a flag required?
    quote: Regardless of whether it is a new feature, refactoring, or bug fix, there is no minimum size that dictates whether a flag is required (either for isolated CLs or for many CLs that form a project/feature). A one line change with potential to impact stability, performance, usability is just as required to use a flag as a multi-thousand line feature.
  - path: docs/flag_guarding_guidelines.md
    heading: Chromium Flag Guarding Guidelines
    quote: This document describes using base::Feature flags which can be remotely set via a server. This applies to both A/B experiments ... (disabled by default) and to kill switches ... (enabled by default).
  - path: docs/configuration.md
    heading: Features
    quote: Features are the best way to add runtime conditional behavior.
  - path: docs/ui/starter_guide.md
    heading: Building Your UI
    quote: Add a [feature flag](/docs/how_to_add_your_feature_flag.md) to gate your UI, and implement it locally.
  - path: docs/release_branch_guidance.md
    heading: Guidelines for Considering Branch Date in Project Planning
    quote: Pay more attention to complexity and structure of CLs around branch points, in case fixes need to be merged back after branch is created.
  - path: docs/ui/starter_guide.md
    heading: Add a feature flag to gate your UI
    quote: Add a feature flag to gate your UI, and implement it locally.
---

**Rule.** Gate newly introduced or experimental behavior—new features,
migrations, user-facing flows, promotional notifications, presentation flows,
and the UI treatments they add—behind a `base::Feature` flag using
`base::FeatureList::IsEnabled(...)` or a dedicated `Is<Feature>Enabled()`
helper; in iOS app extensions, use a plain `Is<Feature>Enabled()` helper
declared in the extension's `feature_flags.h` that reads the value the app
mirrors into the app-group user defaults. Do not hardcode booleans, execute new
logic unconditionally, or enable adaptations for 100% of users under an inverted
kill switch. Behavior-preserving refactors and plain bug fixes are not new
behavior and do not need a flag. Ensure the flag-disabled fallback path strictly
preserves exact existing baseline functionality, default state, and visual
appearance without returning early, omitting necessary initialization, or
executing unflagged fallbacks and altered structures. During refactorings,
feature updates, or release cherry-picks, retain existing feature flag checks
and disabled fallback branches until official platform cleanup, and
conditionally append feature variants to default lists or trial fallbacks only
when enabled. Additionally, gate both the instantiation/styling of
feature-specific UI elements (such as `UIButtonConfiguration`,
`UIVisualEffectView`, or custom subviews) and their attachment into the view
hierarchy (`addSubview:`) so they remain dormant when disabled. Likewise, guard
all state mutations, lifecycle callbacks, buffer accumulation, and cleanup
routines so no state changes or memory allocations occur for disabled features.

**Why.** Feature flags enable safe staged rollouts, experimentation
(FieldTrial/Finch), and emergency remote disabling via kill switches while
confining the blast radius to enrolled users. Unconditionally executing new
feature behavior, exposing unflagged fallback logic, or enabling adaptations for
100% of users under an inverted kill switch alters the default experience for
unflagged users, invalidates A/B experiment comparison metrics, and makes
release cherry-picks unsafe. Conversely, returning early or omitting default
state when a flag is disabled breaks pre-existing functionality in baseline
configurations, and prematurely removing flag checks during refactoring strips
away remote rollback capabilities. Furthermore, unconditionally initializing
experimental UI (such as custom views and visual effect views) or executing
state transitions and buffer accumulation exposes unapproved treatments, leaks
memory, and causes unintended side effects even when the feature is disabled.

## Examples

Bad:

```cpp
// BAD: Early return omits baseline setup when flag is disabled.
if (!IsTabGroupColorOnSurfaceEnabled()) {
  return;  // Drops baseline _dotContainer.color assignment.
}
_dotContainer.color = palette.commonColor;

// BAD: Unconditionally injecting experimental UI.
[self.locationBarSteadyView addCustomViewNextToLocationLabel:geminiBadge];

// BAD: New extension behavior executed unconditionally (no flag check).
return registrationInput.largeBlob ? YES : NO;

// BAD: Unconditionally accumulating state/buffers for experimental feature.
for (auto& query : to_be_committed_redirect_chain_) {
  committed_redirect_chain_.push_back(std::move(query));
}
```

Good:

```cpp
// GOOD: Baseline setup preserved before checking feature flag.
_dotContainer.color = groupColor;
if (IsTabGroupColorOnSurfaceEnabled()) {
  _dotContainer.color = palette.commonColor;
}

// GOOD: Gate UI setup behind the flag.
if (IsGeminiLiveEnabled()) {
  [self.locationBarSteadyView addCustomViewNextToLocationLabel:geminiBadge];
}

// GOOD: In an app extension, use the plain helper declared in the extension's
// feature_flags.h; it reads the value mirrored into the app-group defaults.
if (!IsPasskeyLargeBlobEnabled()) {
  return NO;
}
return registrationInput.largeBlob ? YES : NO;

// GOOD: Guard state mutations and buffer accumulation behind feature flag.
if (base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingAsyncCheck)) {
  for (auto& query : to_be_committed_redirect_chain_) {
    committed_redirect_chain_.push_back(std::move(query));
  }
}
```
