---
name: ios/prefer-enum-class-over-ns-enum
description: In Objective-C++ code, prefer C++ enum class with kCamelCase enumerators over NS_ENUM macros unless Objective-C/Swift interop is required
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Enumerations (`enum class` vs. `NS_ENUM`)
    quote: Because all Objective-C code in `//ios/` is Objective-C++ (`.mm`), prefer C++ scoped enums (`enum class`) over `NS_ENUM`.
---

**Rule.** In iOS Objective-C++ code (`.h`, `.mm`), prefer scoped C++
`enum class` over Objective-C `NS_ENUM` or `NS_CLOSED_ENUM` macros unless
Objective-C or Swift interoperability is strictly required (such as public APIs
or protocols consumed by pure Objective-C or Swift). Name enumerators using
standard C++ `kCamelCase` / `kConstantName` style rather than repeating the enum
type name as a prefix. If a specific underlying integer size is needed, specify
it explicitly using `enum class Foo : NSInteger` (and if an API takes a raw
integer, convert explicitly at the call site, e.g. with
`std::to_underlying(value)`). This rule does not cover bitmask option sets,
which may use `NS_OPTIONS`. Additionally, when declaring file-internal
enumerations and constants in `.mm` implementation files, place them in an
anonymous namespace positioned before the class extension (`@interface`)
declaration rather than between `@interface` and `@implementation`, and omit
redundant class-name prefixes.

**Why.** In Objective-C++ translation units, C++ scoped `enum class` provides
strong type safety without unintended implicit integer conversions, prevents
global symbol table and namespace pollution by scoping enumerator names, allows
forward declaration without typedef wrappers, and eliminates redundant
type-prefixed enumerator names. In contrast, `NS_ENUM` and `NS_CLOSED_ENUM`
expand to C-style unscoped enums designed for Objective-C and Swift runtime
interoperability, which force explicit underlying types and require verbose
prefixes to avoid collisions. Bitmask option sets are exempt because
`NS_OPTIONS` provides the implicit bitwise operations that a scoped enum would
require boilerplate operators to express. Placing file-local enums in an
anonymous namespace above `@interface` keeps declarations consistent and
prevents leaking internal symbols. See `ios/best_practices.md#enumerations-vs`.

## Examples

Bad:

```cpp
// Bad: Using NS_ENUM with prefixed enumerators in Objective-C++ code.
typedef NS_ENUM(NSInteger, ManualFillCredentialType) {
  ManualFillCredentialTypePassword,
  ManualFillCredentialTypePasskey,
};

// Bad: NS_ENUM between @interface and @implementation with redundant class prefix.
@interface PersonalizeGoogleServicesCoordinator () <FooDelegate>
@end

typedef NS_ENUM(NSUInteger, PersonalizeGoogleServicesReauthAction) {
  PersonalizeGoogleServicesReauthActionNone,
  PersonalizeGoogleServicesReauthActionOpenDialog,
};

@implementation PersonalizeGoogleServicesCoordinator
```

Good:

```cpp
// Good: Scoped enum class with kCamelCase enumerators (and explicit underlying type if needed).
enum class ManualFillCredentialType : NSInteger {
  kPassword,
  kPasskey,
};

// Good: Explicit conversion where an API requires an integer identifier.
[model addSectionWithIdentifier:std::to_underlying(SectionIdentifier::kAccounts)];

// Good: File-internal enum in anonymous namespace above @interface with concise names.
namespace {

enum class ActionAfterReauth {
  kNone,
  kOpenDialog,
};

}  // namespace

@interface PersonalizeGoogleServicesCoordinator () <FooDelegate>
@end

@implementation PersonalizeGoogleServicesCoordinator
```
