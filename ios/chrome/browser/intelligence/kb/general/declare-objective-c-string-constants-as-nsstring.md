---
name: ios/declare-objective-c-string-constants-as-nsstring
description: When defining Objective-C string constants, use static or extern NSString* const rather than raw literals, mutable pointers, or C-style char arrays requiring SysUTF8ToNSString
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Objective-C++ style matches the language
    quote: For C functions and constants defined in a namespace, use C++ style, even if most of the file is Objective-C.
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
---

**Rule.** When defining string constants used with Cocoa, Foundation, or UIKit
APIs (such as notification names, dictionary keys, user activity types, KVO
keypaths, or strings compared via isEqualToString:), declare them as
const-qualified Objective-C string pointers (NSString\* const kConstantName)
rather than C-style character arrays (const char[], constexpr char[]),
std::string_view, or mutable pointers (NSString\* kName or const NSString\*
kName). For constants shared across translation units, declare them as extern
NSString\* const kName in header (.h) files and define them in .mm or .m
implementation files, even when scoped within a C++ namespace. For file-local
constants, define them at file scope or within an anonymous namespace using
static NSString\* const kName (or NSString\* const kName inside an anonymous
namespace) with kCamelCase naming rather than declaring mutable local variables
or repeating raw string literals across call sites. Reserve C++ const char[] or
std::string_view for string constants used exclusively in C++ subsystems.

**Why.** Declaring string constants as const char[] or std::string_view forces
Objective-C callers to repeatedly convert them via base::SysUTF8ToNSString(),
introducing runtime transcoding overhead, heap allocations, and call-site
clutter. Declaring them directly as NSString\* const enables direct message
passing and comparison without conversion. Furthermore, declaring pointer
constants as Type\* kName or const Type\* kName leaves the pointer variable
mutable and reassignable at runtime, violating the immutability invariant of
k-prefixed constants. Defining named constants at file scope or in an anonymous
namespace prevents typos across registration and callback sites (such as KVO or
notifications) and provides compiler-checked symbols across translation units.

## Examples

Bad:

```cpp
// Bad: mutable pointer or C-style char array requiring conversion for Cocoa APIs.
NSString* kSerializedPayloadKey = @"op";

// Bad: extern const char[] in header forcing base::SysUTF8ToNSString at call sites.
extern const char kSpotlightActionNewTab[];
if ([action isEqualToString:base::SysUTF8ToNSString(kSpotlightActionNewTab)]) { ... }

// Bad: raw string literals repeated across registration and observer methods.
[webView addObserver:self forKeyPath:@"canGoBack" options:opt context:nil];
if ([keyPath isEqualToString:@"canGoBack"]) { ... }
```

Good:

```cpp
// Good: const pointer after the asterisk ensures the pointer itself is immutable.
static NSString* const kSerializedPayloadKey = @"op";

// Good: extern NSString* const in header, defined in implementation file.
extern NSString* const kSpotlightActionNewTab;
if ([action isEqualToString:kSpotlightActionNewTab]) { ... }

// Good: named constant in anonymous namespace reused across methods.
namespace {
NSString* const kCanGoBackKeyPath = @"canGoBack";
}  // namespace
[webView addObserver:self forKeyPath:kCanGoBackKeyPath options:opt context:nil];
```
