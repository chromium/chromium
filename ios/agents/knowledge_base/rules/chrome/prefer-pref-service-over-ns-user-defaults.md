---
name: ios/prefer-pref-service-over-ns-user-defaults
description: Persist feature and application state in PrefService rather than NSUserDefaults (except for App Group defaults shared with extensions, early startup, or OS settings)
rubric: correctness
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: 'Storing State: Use `PrefService`, Not `NSUserDefaults`'
    quote: New feature code must store persistent state in `PrefService` (profile prefs or application `localState`), not `[NSUserDefaults standardUserDefaults]` (enforced by `PRESUBMIT.py`)
---

**Rule.** Persist application and feature state in `PrefService` (profile prefs
or application `localState`) rather than
`[NSUserDefaults standardUserDefaults]`. `NSUserDefaults` is permitted for App
Group defaults (`app_group::GetGroupUserDefaults()`) shared with app extensions
(such as widgets or share extensions). It is also permitted during early startup
or crash reporting before `ApplicationContext` and `PrefService` are
initialized, and for reading iOS Settings, `Experimental.plist`, or managed
enterprise app configuration.

**Why.** `[NSUserDefaults standardUserDefaults]` is process-global, so it cannot
be partitioned per `ProfileIOS`. Cross-platform Chromium components ignore it,
and it persists across test runs with no hermetic test double. See
`ios/best_practices.md#storing-state-use-prefservice-not-nsuserdefaults`.

## Examples

Bad:

```cpp
// Bad: Storing feature state in NSUserDefaults.
[[NSUserDefaults standardUserDefaults] setBool:YES
                                        forKey:@"FeaturePromoDismissed"];
```

Good:

```cpp
// Good: Persisting feature state via PrefService.
profile_prefs->SetBoolean(prefs::kFeaturePromoDismissed, true);

// Acceptable: Sharing data with an app extension through App Group defaults.
NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
[sharedDefaults setObject:payload
                   forKey:app_group::kSharedExtensionItemKey];
```
