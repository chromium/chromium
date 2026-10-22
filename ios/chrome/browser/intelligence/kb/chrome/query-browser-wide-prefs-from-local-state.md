---
name: ios/query-browser-wide-prefs-from-local-state
rubric: correctness
severity: should_fix
---

**Rule.** Do not query browser-wide (local state) preferences on a
profile-scoped `PrefService` (`ProfileIOS::GetPrefs()`), and do not query user
profile preferences on local state. Query browser-wide preferences (registered
via `RegisterLocalStatePrefs`) exclusively from the local-state `PrefService`
via `GetApplicationContext()->GetLocalState()`, and query user preferences
(registered via `RegisterProfilePrefs`) from `ProfileIOS::GetPrefs()`.
Additionally, when a profile-scoped `KeyedService` requires application-wide
preferences, inject `GetApplicationContext()->GetLocalState()` into the service
from its factory's `BuildServiceInstanceFor` implementation rather than reading
local-state preferences through `profile->GetPrefs()`.

**Why.** Preferences in Chromium on iOS are strictly partitioned between
profile-scoped preferences (`ProfileIOS::GetPrefs()`) and application-wide
preferences (`GetApplicationContext()->GetLocalState()`). Each scope is
registered in a separate `PrefRegistry` and backed by a distinct `PrefService`
instance. Querying a preference on the wrong `PrefService` triggers assertion
crashes (`CHECK`) or fails to find the registered preference. `KeyedService`
implementations requiring local state must receive the local-state
`PrefService*` via dependency injection from `BuildServiceInstanceFor`.

## Examples

Bad:

```cpp
// Bad: Querying a local-state preference from profile-scoped prefs.
bool CredentialProviderService::IsLastUsedProfile() const {
  return profile_name_ == prefs_->GetString(prefs::kLastUsedProfile);
}

// Bad: KeyedService factory fails to inject local state for a service needing it.
std::unique_ptr<KeyedService>
CredentialProviderServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<CredentialProviderService>(
      profile->GetProfileName(), profile->GetPrefs(),
      profile_password_store);
}
```

Good:

```cpp
// Good: Query local-state preference from injected local_state_.
bool CredentialProviderService::IsLastUsedProfile() const {
  return profile_name_ == local_state_->GetString(prefs::kLastUsedProfile);
}

// Good: KeyedService factory injects local-state PrefService alongside profile prefs.
std::unique_ptr<KeyedService>
CredentialProviderServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<CredentialProviderService>(
      profile->GetProfileName(), profile->GetPrefs(),
      GetApplicationContext()->GetLocalState(),
      profile_password_store);
}
```
