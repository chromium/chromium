---
name: ios/clear-syncable-consent-prefs-on-account-change
rubric: correctness
severity: should_fix
---

**Rule.** When a service or agent maintains an account-change cleanup path (such
as clearing account-scoped consent or session preferences in `PrefService` from
`signin::IdentityManager::Observer::OnPrimaryAccountChanged`), update that
cleanup method whenever new related account-scoped preferences are introduced so
all associated keys are cleared together.

**Why.** When a feature explicitly resets local `PrefService` keys upon sign-out
or account switch, omitting newly added related preferences from the cleanup
path leaves stale consent or session state in `PrefService` across account
transitions.

## Examples

Bad:

```cpp
void FooServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  // Missing newly added related prefs `kFooSubConsent` and `kFooIntroShown`.
  pref_service_->ClearPref(prefs::kFooConsent);
}
```

Good:

```cpp
void FooServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  pref_service_->ClearPref(prefs::kFooConsent);
  pref_service_->ClearPref(prefs::kFooSubConsent);
  pref_service_->ClearPref(prefs::kFooIntroShown);
}
```
