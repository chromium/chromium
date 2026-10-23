---
name: ios/guard-unsupported-legacy-methods-under-feature-flags
description: When implementing feature-gated methods, callee branches, or legacy paths, assert expected feature flag preconditions with CHECK instead of DCHECK, redundant runtime branching, or silent early returns
rubric: correctness
severity: should_fix
citations:
  - path: styleguide/c++/checks.md
    heading: Invariant-verification mechanisms
    quote: Prefer `CHECK()` and `NOTREACHED()` over `DCHECK()`s as they ensure that if an invariant fails, the program does not continue in an unexpected state, and we hear about the failure either through a test failure or a crash report.
  - path: styleguide/c++/checks.md
    heading: CHECK(), DCHECK() and NOTREACHED()
    quote: '`CHECK()`, `DCHECK()` and `NOTREACHED()` are all used to ensure that invariants hold. They document (and verify) programmer expectations that either some statement *always* holds true at the point of `(D)CHECK`ing or that a piece of code is unreachable (for `NOTREACHED`).'
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 02. **Remove C++ flag enablement checks**
    quote: Find C++ callsites where the feature enablement is checked. This usually takes the form of `base::FeatureList::IsEnabled(kFeature)` calls or a free `Is<X>Enabled()` helper function that wraps the former.
  - path: styleguide/c++/c++.md
    heading: CHECK(), DCHECK() and NOTREACHED()
    quote: Use the `CHECK()` family of macros to both document and verify invariants.
---

**Rule.** When implementing feature-gated services, factory entry points,
specialized constructors, observer callbacks, or deprecated legacy methods and
fallback branches, assert the expected feature flag precondition directly using
`CHECK` (e.g., `CHECK(IsFeatureEnabled())`,
`CHECK(base::FeatureList::IsEnabled(kFeature))`, or `CHECK(...)` on conditional
registration predicates like
`CHECK(CreateTabHelperOnlyForRealizedWebStates())`); never `DCHECK`. Do not
branch conditionally on redundant `base::FeatureList::IsEnabled(...)` checks
inside a callee or fallback branch when caller dispatch already guarantees that
state, and do not handle disabled flags defensively with runtime error returns
(such as `base::unexpected` or error callbacks), omit assertions, or mask state
internally. Choose between a bare `CHECK(cond)` and
`CHECK(cond, base::NotFatalUntil::M<n>)` based on whether execution can safely
continue if the precondition is violated: use a bare `CHECK(cond)` when
continuing cannot be made safe (such as before dereferencing a feature-only
dependency or when there is no sensible recovery path), and use
`CHECK(cond, base::NotFatalUntil::M<n>)` (setting `M<n>` to the `MAJOR`
milestone in `//chrome/VERSION` plus 3 or 4, never copied from surrounding code)
followed by a graceful fallback (such as an early return) when execution can
safely recover. In architectural migrations and dual-path code:

1. Modern & feature-gated paths: Assert positive enablement at entry of methods,
   setters, and observer callbacks designed to execute only when the feature is
   active. Never rely solely on unverified caller-side gating, and verify
   assertion polarity so that active feature paths do not accidentally assert
   negation (`CHECK(!IsEnabled())`).
2. Legacy & obsolete paths: When a feature flag obsoletes legacy methods,
   fallback branches, UI workflows, or state branches, guard their entry with
   `CHECK(!base::FeatureList::IsEnabled(kFeature))` or
   `CHECK(!Is<Feature>Enabled())`, using the `base::NotFatalUntil::M<n>` form
   plus an early return when the caller can safely continue if the legacy path
   is skipped. Do not leave legacy methods unguarded or branch conditionally on
   redundant feature flags inside obsolete code paths that should never execute
   when the feature is active.

**Why.** Invoking a feature-gated service, factory method, callback, or obsolete
legacy method under the wrong feature flag state represents a caller routing
error or invariant violation, not a recoverable runtime failure. Branching on a
feature flag that caller dispatch already guarantees creates dead else paths and
obscures preconditions, while returning defensive errors (such as
`base::unexpected`) or omitting assertions conceals bugs and risks silent state
drift. Asserting preconditions upfront with `CHECK()` enforces the contract in
every build (a `DCHECK` is not enforced in release builds and is never added),
ensures control flow remains direct, and prevents catastrophic crashes from
inverted assertions (`CHECK(!IsEnabled())`); `base::NotFatalUntil` with a
graceful fallback lets recoverable paths report unexpected violations from the
field without crashing users before the milestone turns fatal. In addition,
explicit `CHECK(!IsEnabled())` assertions in legacy and fallback paths maintain
flag references for grep discoverability during flag deprecation and cleanup.

## Examples

Bad:

```cpp
// Variant 1: Defensive error return or unverified callee instead of asserting precondition.
void FeatureService::Execute(ExecuteCallback callback) {
  if (!IsFeatureEnabled()) {
    std::move(callback).Run(base::unexpected(FeatureError::kDisabled));
    return;
  }
}
void Coordinator::OnContentStateChanged() {
  // Bad: Relies solely on caller gating without callee-side verification.
  _featureService->UpdateFeatureUI();
}

// Variant 2: Redundant branching inside callee or obsolete fallback branch.
bool AuthenticationService::HandleMDMError(id<SystemIdentity> identity,
                                           id<RefreshAccessTokenError> error) {
  if (!base::FeatureList::IsEnabled(
          switches::kHandleMdmErrorsForDasherAccounts) &&
      system_identity_manager->HandleMDMNotification(
          identity, ActiveIdentities(), error, callback)) {
    return true;
  }
  return false;
}

// Variant 3: Inverted assertion polarity on active feature setter.
- (void)setManageOtherDataTitle:(NSString*)title {
  CHECK(!IsPasswordRemovalFromDeleteBrowsingDataEnabled());
  _manageOtherDataTitle = title;
}

// Variant 4: Guarding a legacy path with a DCHECK (not enforced in release
// builds, never added).
- (void)showLegacyPromo {
  DCHECK(!IsNewPromoEnabled());
  [self presentLegacyPromo];
}
```

Good:

```cpp
// Variant 1: Bare CHECK when continuing cannot be made safe (required callback
// or feature-only dependency dereference).
void FeatureService::Execute(ExecuteCallback callback) {
  CHECK(IsFeatureEnabled());
}
void Coordinator::OnContentStateChanged() {
  CHECK(IsSuperFeatureEnabled());
  _featureService->UpdateFeatureUI();
}

// Variant 2: Assert expected flag precondition directly with CHECK instead of branching.
bool AuthenticationService::HandleMDMError(id<SystemIdentity> identity,
                                           id<RefreshAccessTokenError> error) {
  if (system_identity_manager->HandleMDMNotification(
          identity, ActiveIdentities(), error, callback)) {
    CHECK(!base::FeatureList::IsEnabled(
        switches::kHandleMdmErrorsForDasherAccounts));
    return true;
  }
  return false;
}

// Variant 3: Correct assertion polarity; NotFatalUntil + early return when recoverable.
- (void)setManageOtherDataTitle:(NSString*)title {
  CHECK(IsPasswordRemovalFromDeleteBrowsingDataEnabled(),
        base::NotFatalUntil::M161);
  if (!IsPasswordRemovalFromDeleteBrowsingDataEnabled()) {
    return;
  }
  _manageOtherDataTitle = title;
}

// Variant 4: Legacy method guarded with NotFatalUntil (MAJOR + 3/4) and a
// graceful early-return fallback.
- (void)showLegacyPromo {
  CHECK(!IsNewPromoEnabled(), base::NotFatalUntil::M161);
  if (IsNewPromoEnabled()) {
    return;
  }
  [self presentLegacyPromo];
}
```
