---
name: ios/coordinator-use-self-profile
description: Reuse held ProfileIOS properties or ivars instead of dereferencing browser->GetProfile(), retrieve services directly in ChromeCoordinator without initializer injection, and cache lookups
rubric: design
severity: nit
citations:
  - path: docs/ios/objects.md
    heading: ProfileIOS
    quote: A special case of base::SupportsUserData::Data is the KeyedService. They are managed by the ProfileKeyedServiceFactoryIOS infrastructure.
---

**Rule.** When accessing a dependency that is already stored as an ivar or
property on self (such as `_profile` or `self.profile`), reuse the existing
member directly instead of re-fetching it through another object reference (such
as calling `self.browser->GetProfile()`), and avoid creating redundant local
variables that duplicate or shadow held dependencies. In `ChromeCoordinator`
subclasses specifically, access the profile directly via `self.profile` (or
`self.profile->GetOriginalProfile()`) rather than dereferencing
`browser->GetProfile()`. Do not pass profile-scoped services, keyed services, or
data managers (such as `PrefService*` or `EntityDataManager*`) into coordinator
initializers or through mediator delegate callbacks; coordinators have direct
access to `self.profile` and must retrieve them directly or via their respective
service factories. When accessing `self.profile` multiple times in a method,
cache it locally (`ProfileIOS* profile = self.profile;`). When initializing
dependencies with the non-incognito profile, cache
`self.profile->GetOriginalProfile()` in a local variable (`originalProfile`) and
pass it consistently to all intended service factories.

**Why.** Re-fetching held dependencies through indirect object references
introduces unnecessary indirection, adds visual noise, and leads to inconsistent
dependency usage across methods. ChromeCoordinator provides a built-in
`self.profile` convenience property with safety assertions; dereferencing
`browser` directly bypasses coordinator encapsulation. Passing profile-scoped
services or data managers through coordinator initializers or mediator delegate
callbacks clutters public interfaces, forces callers to forward dependencies
unnecessarily, and violates coordinator encapsulation when coordinators already
have direct access. Furthermore, repeatedly calling `self.profile` incurs
redundant Objective-C message dispatches, and mixing `self.profile` with
`GetOriginalProfile()` creates inconsistent service states and potential privacy
leaks in incognito sessions.

## Examples

Bad:

```cpp
// Bad: re-fetching ProfileIOS through browser instead of reusing held _profile ivar:
ProfileIOS* profile = self.browser->GetProfile();
BrowserList* browserList = BrowserListFactory::GetForProfile(profile);

// Bad: passing profile-scoped services into coordinator initializers or mediator callbacks:
- (instancetype)initWithBaseNavigationController:(UINavigationController*)navigationController
                                         browser:(Browser*)browser
                               entityDataManager:(autofill::EntityDataManager*)entityDataManager;

// Bad: dereferencing browser directly and mixing self.profile with original profile:
ProfileIOS* profile = self.browser->GetProfile()->GetOriginalProfile();
PrefService* prefService = profile->GetPrefs();
feature_engagement::Tracker* tracker =
    feature_engagement::TrackerFactory::GetForProfile(self.profile);

// Bad: repeatedly invoking self.profile instead of caching locally:
PrefService* prefs = self.profile->GetPrefs();
commerce::ShoppingService* shoppingService =
    commerce::ShoppingServiceFactory::GetForProfile(self.profile);
```

Good:

```cpp
// Good: reusing held _profile ivar directly:
BrowserList* browserList = BrowserListFactory::GetForProfile(_profile);

// Good: retrieve profile-scoped service inside coordinator via self.profile:
- (instancetype)initWithBaseNavigationController:(UINavigationController*)navigationController
                                         browser:(Browser*)browser;
- (void)start {
  autofill::EntityDataManager* entityDataManager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(self.profile);
}

// Good: cache originalProfile from self.profile and pass it consistently:
ProfileIOS* originalProfile = self.profile->GetOriginalProfile();
PrefService* prefService = originalProfile->GetPrefs();
feature_engagement::Tracker* tracker =
    feature_engagement::TrackerFactory::GetForProfile(originalProfile);

// Good: cache self.profile in a local variable for repeated lookups:
ProfileIOS* profile = self.profile;
PrefService* prefs = profile->GetPrefs();
commerce::ShoppingService* shoppingService =
    commerce::ShoppingServiceFactory::GetForProfile(profile);
```
