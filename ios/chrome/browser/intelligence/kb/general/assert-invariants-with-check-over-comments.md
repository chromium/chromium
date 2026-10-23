---
name: ios/assert-invariants-with-check-over-comments
description: Assert preconditions, non-null invariants, and prerequisite feature flags upfront with CHECK (or CHECK with base::NotFatalUntil::M<n> and a safe fallback) rather than comments, DCHECK, or unverified fallbacks
rubric: correctness
severity: should_fix
citations:
  - path: styleguide/c++/checks.md
    heading: CHECK(), DCHECK() and NOTREACHED()
    quote: '`CHECK()`, `DCHECK()` and `NOTREACHED()` are all used to ensure that invariants hold. They document (and verify) programmer expectations that either some statement *always* holds true at the point of `(D)CHECK`ing or that a piece of code is unreachable (for `NOTREACHED`).'
---

**Rule.** When relying on preconditions, required dependencies, API contract
guarantees, or runtime invariants, assert expectations explicitly using
`CHECK()` (including relational variants like `CHECK_EQ()`, `CHECK_NE()`,
`CHECK_LT()`, `CHECK_LE()`, `CHECK_GT()`, or `CHECK_GE()`) rather than
documenting them in comments or leaving them unverified. Never add new
`DCHECK()`s; the only acceptable `DCHECK` is
`DCHECK_CALLED_ON_VALID_SEQUENCE()`, which has no `CHECK` equivalent. Choose
between a bare `CHECK` and `base::NotFatalUntil` based on whether execution can
safely continue if the invariant is violated: use a bare `CHECK(cond)` when
continuing cannot be made safe (for example, immediately before dereferencing a
required pointer with no sensible recovery path), and use
`CHECK(cond, base::NotFatalUntil::M<n>)` followed by graceful fallback handling
(such as an early return) when execution can safely recover, setting `M<n>` to
the `MAJOR` milestone in `//chrome/VERSION` plus 3 or 4 (never copied from
surrounding code). In particular:

- Assert non-null pointer arguments or valid non-sentinel values before
  dereferencing or use.
- Verify prerequisite states before initiating dependent actions.
- Assert required dependencies before passing them into downstream helpers that
  accept null gracefully and would otherwise silently fall back.

Additionally, place function preconditions and invariant assertions (such as
verifying feature flags, caller arguments, or object state) at the top of the
function or method entry before any conditional branching, intermediate setup,
or early returns, unless the assertion inherently depends on preceding
computation. Do not allow early exits or guard clauses to bypass verifying
preconditions that must hold for all invocations of the function. Additionally,
when implementing accessors or methods valid only under a specific operational
scope, mode, or variant, assert that precondition at entry using `CHECK()` or
`CHECK_EQ()` rather than silently returning `nullptr` or uninitialized state.
Similarly, when implementing getter functions for feature parameters
(`BASE_FEATURE_PARAM` or `base::FeatureParam`) that depend on a prerequisite or
sub-feature flag, assert that the prerequisite flag is enabled via
`CHECK(Is<Feature>Enabled())` (or guard with a safe fallback default if callers
invoke it conditionally) rather than returning unverified values or defaults
that mask caller contract violations.

**Why.** Comments merely express programmer assumptions, and unasserted
preconditions or dereferenced null pointers cause undefined behavior, memory
corruption, or invalid execution when callers pass unexpected nulls or
downstream helpers mask missing dependencies with default fallbacks. Placing
precondition assertions after early returns or intermediate conditional checks
creates bypass paths where invalid callers or disabled feature environments
escape detection without verification, masking logic bugs. Furthermore, silently
returning null, uninitialized state, or configured parameter values from a
mode-, scope-, or feature-restricted accessor conceals caller contract
violations and allows the application to proceed in an invalid state; while
`base::FeatureParam::Get()` automatically defaults when its own parent feature
is disabled, it does not check dependent sub-feature or prerequisite flags.
Asserting expectations with `CHECK()` documents contracts and ensures violations
fail fast in every build configuration (a `DCHECK` is compiled out of release
builds, so it never surfaces the violation in production crash reports),
preventing corrupted state propagation and surfacing defects deterministically
in tests and crash reports.

## Examples

Bad:

```cpp
// Variant 1: Comment documenting invariant instead of asserting pointer.
// Profile node must not be null and loaded profiles cannot be off-the-record.
const std::string& GetRegularProfileName(ProfileIOS* profile) {
  return profile->GetProfileName();
}

// Variant 2: Precondition assertion placed after conditional early return.
void HandleNotification(NSDictionary* user_info) {
  if (!user_info[kProfileKey]) {
    return;
  }
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());
}

// Variant 3: Scope-restricted accessor or feature param getter unchecked.
ProfileIOS* NotificationClient::GetProfile() const {
  return profile_.get();  // Silently returns null/invalid if mode is wrong.
}

base::TimeDelta GetActorObservationDelayTimeout() {
  return kObservationDelayTimeout.Get();  // Prerequisite feature unchecked.
}
```

Good:

```cpp
// Variant 1: Bare CHECK when continuing cannot be made safe (required reference return).
const std::string& GetRegularProfileName(ProfileIOS* profile) {
  CHECK(profile);
  CHECK(!profile->IsOffTheRecord());
  return profile->GetProfileName();
}

// Variant 2: Assert precondition at entry; use NotFatalUntil + early return when recoverable.
void HandleNotification(NSDictionary* user_info) {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled(),
        base::NotFatalUntil::M161);
  if (!IsMultiProfilePushNotificationHandlingEnabled() ||
      !user_info[kProfileKey]) {
    return;
  }
}

// Variant 3: Assert operational scope precondition and prerequisite flag.
ProfileIOS* NotificationClient::GetProfile() const {
  CHECK_EQ(client_scope_, NotificationClientScope::kPerProfile);
  return profile_.get();
}

base::TimeDelta GetActorObservationDelayTimeout() {
  CHECK(IsPageStabilityEnabled());
  return kObservationDelayTimeout.Get();
}
```
