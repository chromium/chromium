---
name: ios/prefer-forward-declarations-over-imports-in-headers
description: In headers and source/test files, forward-declare types referenced by pointer or signature instead of including headers, reserving imports for complete types and avoiding unnecessary BUILD.gn dependencies
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Headers, Imports, and Declaration Order
    quote: Forward-declare classes (`class Foo;`, `@class Bar;`) and protocols (`@protocol Baz;`) in headers instead of `#import`ing their headers whenever the full definition is not needed (i.e., when the header does not inherit from the type, conform to the protocol, or embed a C++ value/smart-pointer member requiring the complete type).
  - path: styleguide/c++/c++.md
    heading: 'Forward declarations vs. #includes'
    quote: Unlike the Google style guide, Chromium style prefers forward declarations to `#includes` where possible. This can reduce compile times and result in fewer files needing recompilation when a header changes.
  - path: styleguide/c++/c++-dos-and-donts.md
    heading: Minimize Code in Headers
    quote: "You can't use forward declarations when: * Creating an instance of the type (e.g. `MyClass obj;`). The compiler needs to know the type's size to allocate memory."
  - path: ios/tools/forward_declarations/README.md
    heading: Overview
    quote: 'By replacing full header imports with lightweight C++ forward declarations (`class` / `struct`) and Objective-C forward declarations (`@class` / `@protocol`), we: 1. **Decouple Interface Dependencies** — Reducing downstream build breaks. 2. **Speed up Compilation** — Siso/Ninja spend less time parsing redundant AST nodes. 3. **Maintain Clean Scope** — Move implementation imports directly to `.mm` and `.cc` files where they belong.'
---

**Rule.** In C++ and Objective-C/Objective-C++ headers and implementation/test
files, prefer forward declarations (`class Foo;`, `struct Bar;`, `@class Baz;`,
`@protocol Qux;`, or `enum class Quux;`) over `#import`ing or `#include`ing
defining headers when types are only referenced by pointer, reference, protocol
qualifier (`id<Qux>`), or signature:

1. **In header files (`.h`, `.hpp`)**: Forward-declare types referenced by
   pointer, reference, protocol, or method/property signatures. Remove unused
   `#import` or `#include` directives not referenced in the header interface,
   and do not retain `#import` directives when forward declarations are present;
   move complete `#import` and `#include` directives into the implementation
   file (`.mm`, `.cc`). Directly `#import` or `#include` defining headers in
   `.h` only when complete type definitions are strictly required—such as for
   superclass inheritance, protocol adoption/conformance
   (`@interface Foo : Bar <Qux>`), inline member access, by-value storage or
   parameters/returns, value templates like `std::optional<T>`, or C++ type
   aliases (`using` or `typedef`, such as `base::PassKey<T>`) which cannot be
   forward-declared.
2. **In source and test files (`.cc`, `.mm`, `.cpp`)**: Do not `#include` or
   `#import` a type's definition header when the type is only used by pointer or
   reference without dereferencing it, calling its methods, or accessing its
   members. When a type is only referenced in a function signature, callback
   (`base::BindRepeating`, `base::BindOnce`), or mock return type, provide a
   minimal forward declaration (`namespace ns { class Type; }` or
   `@class Type;`) in the source or test file rather than importing the full
   header and adding unnecessary `deps` in `BUILD.gn`.

**Why.** Chromium style explicitly prefers forward declarations over `#includes`
and `#imports` where possible. Including full definition headers inflates
translation units, slows compilation, and introduces tight coupling. In headers,
unnecessary imports force downstream files across the codebase to recompile
whenever dependencies change, whereas forward declarations decouple interface
dependencies and reduce AST parsing overhead (though superclasses, adopted
protocols, value templates like `std::optional<T>`, and type aliases strictly
require full definitions). In source and test files, importing unneeded headers
often forces adding unnecessary target dependencies to `deps` in `BUILD.gn`,
creating tight build graph coupling and causing widespread rebuild churn
whenever the included header changes. See `ios/best_practices.md` ("Headers,
Imports, and Declaration Order").

## Examples

Bad:

```cpp
// Variant 1: Header (.h) imports pointers/protocols or mis-forward-declares type aliases
#import "ios/chrome/browser/tab_switcher/util/tab_group_color_palette.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"

namespace web {
class WebViewPassKey;  // Error: Type alias (`using`) cannot be forward-declared.
}

@interface GroupTabView : UIView
@property(nonatomic) TabGroupColorPalette* colorPalette;
@property(nonatomic, weak) id<QuickDeleteCommands> quickDeleteHandler;
@end

// Variant 2: Source/test (.mm) imports a header only to name a callback return type, adding a BUILD.gn dep
#import "components/variations/service/variations_service.h"

/*variations_service_getter=*/
base::BindRepeating([]() -> variations::VariationsService* { return nullptr; });
```

Good:

```cpp
// Variant 1: Header (.h) uses forward declarations; imports type aliases
#import "ios/web/web_state/web_view_pass_key.h"  // Type alias requires definition header.

@class TabGroupColorPalette;
@protocol QuickDeleteCommands;

@interface GroupTabView : UIView
@property(nonatomic) TabGroupColorPalette* colorPalette;
@property(nonatomic, weak) id<QuickDeleteCommands> quickDeleteHandler;
@end

// Variant 2: Source/test (.mm) provides its own minimal forward declaration
namespace variations {
class VariationsService;
}

/*variations_service_getter=*/
base::BindRepeating([]() -> variations::VariationsService* { return nullptr; });
```
