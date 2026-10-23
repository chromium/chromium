---
name: ios/verify-signin-enabled-before-account-menu
description: When presenting account menus or triggering account-switching flows, verify AuthenticationService::SigninEnabled() alongside sign-in state to avoid crashing during asynchronous sign-out
rubric: correctness
severity: should_fix
---

**Rule.** Before presenting the account menu or triggering account-switching and
sign-in flows, verify `AuthenticationService::SigninEnabled()` in addition to
checking whether the user is signed in (e.g., via `-isUserSignedIn` or
`HasPrimaryIdentity()`). Do not assume that a signed-in user implies sign-in
remains currently enabled.

**Why.** When sign-in is disabled dynamically (such as by enterprise policy),
clearing profile data and signing out occurs asynchronously. During this window,
the user remains signed in while sign-in is disabled; attempting to present
`AccountMenuCoordinator` or start sign-in triggers
`CHECK(authService->SigninEnabled())` and crashes the browser.

## Examples

Bad:

```cpp
- (BOOL)isAccountSwitchEligible {
  return [self isUserSignedIn] && _geminiService;
}
```

Good:

```cpp
- (BOOL)isAccountSwitchEligible {
  return [self isUserSignedIn] && _geminiService &&
         _authenticationService->SigninEnabled();
}
```
