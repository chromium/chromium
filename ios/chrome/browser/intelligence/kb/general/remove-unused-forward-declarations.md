---
name: ios/remove-unused-forward-declarations
description: In header files, remove unused forward declarations and redundant declarations of imported types, but retain declarations for types still used in private or overridden methods
rubric: readability
severity: should_fix
citations:
  - path: styleguide/c++/c++-dos-and-donts.md
    heading: Minimize Code in Headers
    quote: "Remove #includes you don't use. Unfortunately, Chromium lacks include-what-you-use (\"IWYU\") support, so there's no tooling to do this automatically. Look carefully when refactoring."
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 09. **Delete unused imports and build deps**
    quote: Remove `#include`s, `#import`s, and forward declarations that are no longer needed.
  - path: styleguide/c++/c++.md
    heading: 'Forward declarations vs. #includes'
    quote: Unlike the Google style guide, Chromium style prefers forward declarations to `#includes` where possible. This can reduce compile times and result in fewer files needing recompilation when a header changes.
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: '09. **Delete unused imports and build deps**:'
    quote: Remove `#include`s, `#import`s, and forward declarations that are no longer needed.
---

**Rule.** Do not add or retain unneeded or redundant forward declarations
(`class`, `struct`, `template <...> class`, `enum class`, `@class`, `@protocol`)
in header files. When editing headers, refactoring method signatures, return
types, interface properties, or struct/class members, or relocating
implementation details to source files, audit surrounding forward declarations
and remove any symbols no longer referenced in the interface, typedefs, or
signatures. Furthermore, eliminate forward declarations for types whose
definitions are already `#include`d or `#import`ed directly in the header.
Conversely, do not remove forward declarations for types that remain referenced
anywhere in the file (such as in private methods, private member variables, or
base class overrides); never rely on fragile transitive includes for types that
remain in use. Additionally, when removing obsolete features, delete vestigial
handler setters (`Set*Handler`) and unreferenced member variables (`*_handler_`)
along with their orphaned forward declarations.

**Why.** Unused forward declarations add visual noise, clutter header files with
dead code, obscure actual file dependencies, and mislead maintainers into
believing the interface couples to types that are no longer used. Redundant
forward declarations for types already provided by a direct `#include` or
`#import` duplicate symbols and create confusion about whether the full
definition is available. Conversely, removing forward declarations for types
still referenced in private methods or overrides violates declare-what-you-use
self-containment and leaves headers vulnerable to breakages from transitive
include churn.

## Examples

Bad:

```cpp
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// Bad: Redundant forward declaration of a type defined by the imported header above.
class ProfileIOS;

// Bad: Unreferenced declarations left behind after refactoring.
@class ObsoleteViewController;
@protocol SnackbarCommands;

class FooTabHelper : public web::WebStateObserver {
 public:
  // Bad: Orphaned handler left after feature removal.
  void SetSnackbarCommandsHandler(id<SnackbarCommands> handler);
};

// Bad: Forward declaration for KeyedService omitted even though it is used in
// the private override.
class FooServiceFactory : public ProfileKeyedServiceFactoryIOS {
 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};
```

Good:

```cpp
// Good: ProfileIOS is only named in a signature, so a forward declaration
// replaces the import.
class KeyedService;
class ProfileIOS;

// Good: Unreferenced forward declarations and orphaned handler methods removed.
class FooTabHelper : public web::WebStateObserver {};

// Good: Forward declarations retained for the types used in the private
// override.
class FooServiceFactory : public ProfileKeyedServiceFactoryIOS {
 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};
```
