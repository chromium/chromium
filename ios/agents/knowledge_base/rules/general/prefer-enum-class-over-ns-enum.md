---
name: ios/prefer-enum-class-over-ns-enum
description: Prefer C++ enum class with kCamelCase enumerators over NS_ENUM in Objective-C++ code, except for NS_OPTIONS bitmasks or pure Objective-C/Swift interop
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Enumerations (`enum class` vs. `NS_ENUM`)
    quote: 'Because all Objective-C code in `//ios/` is Objective-C++ (`.mm`), prefer C++ scoped enums (`enum class`) over `NS_ENUM`:'
---

**Rule.** Prefer C++ `enum class` over `NS_ENUM` or `NS_CLOSED_ENUM` in
Objective-C++ code (`.h`, `.mm`) unless pure Objective-C or Swift
interoperability is required. Name enumerators in `kCamelCase` (`kConstantName`)
instead of prefixing them with the enum type name. Specify an explicit
underlying type (`enum class Foo : NSInteger`) when needed, and convert
explicitly (such as with `std::to_underlying`) only where an API requires a raw
integer. Bitmask option sets may continue to use `NS_OPTIONS`.

**Why.** All Objective-C code in `//ios/` is Objective-C++ (`.mm`), so
`enum class` is always available. Its enumerators are scoped, so they need no
type-name prefix, and it blocks implicit integer conversions. `NS_ENUM` is
needed only when Swift or plain Objective-C consumes the header, and
`NS_OPTIONS` remains the convenient choice for bitmasks. See
`ios/best_practices.md#enumerations-enum-class-vs-ns_enum`.

## Examples

Bad:

```cpp
// Bad: Using NS_ENUM with prefixed enumerators in Objective-C++ code.
typedef NS_ENUM(NSInteger, FooItemType) {
  FooItemTypePrimary,
  FooItemTypeSecondary,
};
```

Good:

```cpp
// Good: Scoped enum class with kCamelCase enumerators (add `: NSInteger` when
// an explicit underlying type is needed).
enum class FooItemType {
  kPrimary,
  kSecondary,
};
```
