---
name: ios/prefer-pref-service-over-ns-user-defaults
description: When persisting settings on iOS, prefer PrefService over NSUserDefaults, and migrate existing NSUserDefaults data in browser_prefs to prevent data loss
rubric: correctness
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: 'Storing State: Use `PrefService`, Not `NSUserDefaults`'
    quote: New feature code must store persistent state in `PrefService` (profile prefs or application `localState`), not `[NSUserDefaults standardUserDefaults]` (reserve `NSUserDefaults` for App Group / extension communication or reading legacy keys during migration).
  - path: chrome/browser/prefs/README.md
    heading: Prefs
    quote: Prefs is meant to store lightweight state that reflects user preferences (e.g. chrome://settings, position of windows on last exit, etc.). Browser-wide prefs are stored in Local State (`g_browser_process->local_state()`) and per-profile prefs are stored in Preferences (`Profile::GetPrefs()`).
---

**Rule.** Persist application and feature state via `PrefService` (profile prefs
or the application `localState`) rather than
`[NSUserDefaults standardUserDefaults]`. When migrating existing persistent
state or preferences from `NSUserDefaults` to `PrefService`, or renaming keys,
implement an explicit data migration in `browser_prefs.mm` (such as in
`MigrateObsoleteProfilePrefs()`, `MigrateObsoleteLocalStatePrefs()`, or
`MigrateObsoleteUserDefault()`) and clear the obsolete `NSUserDefaults` key to
prevent data loss. `NSUserDefaults` is acceptable only where the platform or
execution environment requires it: App Group shared defaults
(`app_group::GetGroupUserDefaults()`) for sharing data with app extensions
(widgets, the Credential Provider Extension, Share/Open extensions) that cannot
access the main app's `PrefService`; state read or cached during early startup
or crash reporting before `ApplicationContext` and `PrefService` are
initialized; and reading iOS Settings or Experimental settings
(`Experimental.plist`) or managed enterprise app configuration.

**Why.** `NSUserDefaults` is a process-global singleton that cannot partition
data per `ProfileIOS` for multi-identity support, is not read by cross-platform
Chromium components that use `PrefService`, and has no hermetic test double, so
state leaks across test runs and causes flake. Moving existing settings to
`PrefService` without an explicit migration in `browser_prefs.mm` causes
existing installations to reset to default values and lose user data on upgrade.
App extensions, early startup, and OS-level settings are exempt because
`PrefService` is unavailable or not yet initialized in those contexts. See
`ios/best_practices.md` ("Storing State: Use PrefService, Not NSUserDefaults")
and `chrome/browser/prefs/README.md#migrating-a-pref`.

## Examples

Bad:

```cpp
// Bad: Storing feature state in NSUserDefaults.
[[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"FeaturePromoDismissed"];

// Bad: Reading from PrefService after migration without migrating existing NSUserDefaults value.
BOOL alertShown =
    profile_prefs->GetBoolean(policy::policy_prefs::kSyncDisabledAlertShown);
```

Good:

```cpp
// Good: Persisting feature state via PrefService.
profile_prefs->SetBoolean(prefs::kFeaturePromoDismissed, true);

// Good: Migrating existing NSUserDefaults in MigrateObsoleteProfilePrefs(prefs):
NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
if ([defaults objectForKey:kSyncDisabledAlertShownKey]) {
  prefs->SetBoolean(policy::policy_prefs::kSyncDisabledAlertShown,
                    [defaults boolForKey:kSyncDisabledAlertShownKey]);
  [defaults removeObjectForKey:kSyncDisabledAlertShownKey];
}

// Acceptable: Sharing data with an app extension through App Group defaults.
NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
[sharedDefaults setObject:capabilities
                   forKey:app_group::kChromeCapabilitiesPreference];
```
