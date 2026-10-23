---
name: ios/name-browser-state-path-as-state-path
description: Name web::BrowserState instances and derived paths browser_state and state_path, and name ProfileIOS instances and derived paths profile and profile_path, using ProfileIOS::FromBrowserState at the ios/web boundary
rubric: design
severity: should_fix
citations:
  - path: ios/web/README.md
    heading: Overview
    quote: "BrowserState: Represents a user's session (analogous to content::BrowserContext)."
  - path: docs/ios/objects.md
    heading: ProfileIOS
    quote: ProfileIOS objects correspond to a user browsing session. Each regular ProfileIOS also owns an incognito ProfileIOS. There can be more than one regular ProfileIOS loaded at the same time.
---

**Rule.** Across all iOS code, when naming `web::BrowserState` or
`web::FakeBrowserState` instances, parameters, and derived filesystem paths
(`GetStatePath()`), name them `browser_state`, `fake_browser_state`, or
`state_path` rather than `profile`, `fake_profile`, or `profile_path`.
Conversely, name `ProfileIOS` parameters, member variables, and local variables
`profile` (or with descriptive qualifiers such as `otr_profile`,
`incognito_profile`, or `mainProfile`) rather than `state` or `browser_state`,
ensuring header declaration parameter names match implementation definitions,
and downcast from a `web::BrowserState*` with
`ProfileIOS::FromBrowserState(...)`.

**Why.** `web::BrowserState` is a lower-level session abstraction in `ios/web`
(analogous to `content::BrowserContext`) that is independent of Chrome profiles,
whereas `ProfileIOS` represents the Chrome-level profile. Naming
`web::BrowserState` instances or their filesystem paths (`GetStatePath()`)
`profile` or `profile_path` violates layer separation, while retaining legacy
`browser_state` naming for `ProfileIOS` creates misleading type mismatches and
blurs the boundary between the web layer and the Chrome layer.

## Examples

Bad:

```cpp
// web::BrowserState or GetStatePath() named profile or profile_path.
web::FakeBrowserState fake_profile;
const base::FilePath& profile_path = browser_state->GetStatePath();

// ProfileIOS parameter named browser_state.
static FooService* GetForProfile(ProfileIOS* browser_state);
```

Good:

```cpp
// web::BrowserState and GetStatePath() named browser_state and state_path.
web::FakeBrowserState fake_browser_state;
const base::FilePath& state_path = browser_state->GetStatePath();

// ProfileIOS parameter named profile.
static FooService* GetForProfile(ProfileIOS* profile);
```
