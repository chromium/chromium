---
name: ios/all-ios-source-files-must-be-objective-cpp
description: In iOS source files, use the .mm extension rather than plain .m, and compile C++ files accessing Objective-C headers or ApplicationContext as .mm
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Language Choice
    quote: In `//ios/`, "Objective-C" always means Objective-C++ (`.mm`), which allows freely mixing Objective-C and C++. Plain Objective-C (`.m`) is not used outside `//ios/third_party/`.
---

**Rule.** In `ios/`, author Objective-C source files with the `.mm` extension
(Objective-C++) rather than plain `.m`; plain `.m` files are not supported
outside third-party libraries. Additionally, when a C++ source file (`.cc`)
includes Objective-C headers (such as `application_context.h` or Apple platform
frameworks) or calls `GetApplicationContext()`, rename and compile the source
file as Objective-C++ (`.mm`).

**Why.** Chromium's iOS codebase is built on Objective-C++ to enable
interoperability between C++ components and Cocoa APIs. Plain `.m` files cannot
leverage C++ features, namespaces, or standard libraries. Conversely, pure C++
translation units (`.cc`) fail compilation when including headers that contain
Objective-C declarations (such as `application_context.h`, which imports
`<Foundation/Foundation.h>` and declares Objective-C protocols). Compiling as
`.mm` ensures full language compatibility across both C++ and Objective-C. See
`ios/best_practices.md#language-choice`.

## Examples

Bad:

```cpp
// file: ios/chrome/browser/ui/foo/foo_view.m
#import "ios/chrome/browser/ui/foo/foo_view.h"
@implementation FooView
@end

// file: ios/chrome/browser/bookmarks/model/bookmark_client_impl.cc
#include "ios/chrome/browser/shared/model/application_context/application_context.h"
...
GetApplicationContext()->GetOSCryptAsync()->GetInstance(std::move(callback));
```

Good:

```cpp
// file: ios/chrome/browser/ui/foo/foo_view.mm
#import "ios/chrome/browser/ui/foo/foo_view.h"
@implementation FooView
@end

// file: ios/chrome/browser/bookmarks/model/bookmark_client_impl.mm
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
...
GetApplicationContext()->GetOSCryptAsync()->GetInstance(std::move(callback));
```
