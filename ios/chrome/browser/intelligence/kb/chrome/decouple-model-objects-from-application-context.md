---
name: ios/decouple-model-objects-from-application-context
description: In shared model classes and core model build targets, do not query or depend on ApplicationContext or owning managers; extract helpers to separate utils targets
rubric: design
severity: should_fix
citations:
  - path: docs/ios/objects.md
    heading: ProfileIOS
    quote: The ProfileIOS objects are owned by the ProfileManagerIOS that can be accessed via the ApplicationContext.
---

**Rule.** Do not make lower-scoped model classes (such as tab-, browser-, or
profile-scoped objects like `ProfileIOS` or `Browser`) query or depend upward on
their owning managers or broader-scoped containers (such as `ProfileManagerIOS`
or `ApplicationContext`), and do not add `ApplicationContext` as a dependency of
foundational model build targets (such as `:profile`). When an operation
requires access to both a lower-scoped model object and broader manager- or
application-level state, implement it as a standalone free function in a
separate utility sub-target (such as `:utils`) rather than a member function or
core target dependency.

**Why.** Lower-scoped model objects are owned by higher-level managers accessed
via `ApplicationContext`. Calling upward into broader singletons or containers
from model member functions or adding `application_context` dependencies to
foundational model targets violates scope layering, leaks application singletons
transitively to downstream consumers, and risks circular build dependencies.

## Examples

Bad:

```cpp
// In profile_ios.mm (member function in :profile target):
bool ProfileIOS::IsPersonalProfile() {
  return GetProfileName() == GetApplicationContext()
                                 ->GetProfileManager()
                                 ->GetProfileAttributesStorage()
                                 ->GetPersonalProfileName();
}

// In ios/chrome/browser/shared/model/profile/BUILD.gn:
source_set("profile") {
  sources = [ "profile_ios.mm", ... ]
  deps = [
    "//ios/chrome/browser/shared/model/application_context",
    ...
  ]
}
```

Good:

```cpp
// In profile_ios_util.mm (free function in separate :utils target):
bool IsPersonalProfile(ProfileIOS* profile) {
  return profile->GetProfileName() == GetApplicationContext()
                                         ->GetProfileManager()
                                         ->GetProfileAttributesStorage()
                                         ->GetPersonalProfileName();
}

// In ios/chrome/browser/shared/model/profile/BUILD.gn:
source_set("profile") {
  sources = [ "profile_ios.mm", ... ]
  deps = [ ... ]  # Kept strictly decoupled from application_context.
}

source_set("utils") {
  sources = [ "profile_ios_util.mm", ... ]
  deps = [
    ":profile",
    "//ios/chrome/browser/shared/model/application_context",
  ]
}
```
