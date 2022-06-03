# Chromium Objective-C and Objective-C++ style guide

_For other languages, please see the [Chromium style guides](https://chromium.googlesource.com/chromium/src/+/main/styleguide/styleguide.md)._

Chromium follows the
[Google Objective-C style guide](https://google.github.io/styleguide/objcguide.html)
unless an exception is listed below.

A checkout should give you
[clang-format](https://chromium.googlesource.com/chromium/src/+/main/docs/clang_format.md)
to automatically format Objective-C and Objective-C++ code. By policy, Clang's
formatting of code should always be accepted in code reviews. If Clang's
formatting doesn't follow this style guide, file a bug.

## Line length

For consistency with the 80 character line length used in Chromium C++ code,
Objective-C and Objective-C++ code also has an 80 character line length.

## Chromium C++ style

Where appropriate, the [Chromium C++ style](../c++/c++.md) style guide applies
to Chromium Objective-C and (especially) Objective-C++

## Code Formatting

Use `nil` for null pointers to Objective-C objects, and `nullptr` for C++
objects.

## Objective-C++ style matches the language

Within an Objective-C++ source file, follow the style for the language of the
function or method you're implementing.

In order to minimize clashes between the differing naming styles when mixing
Cocoa/Objective-C and C++, follow the style of the method being implemented.

For code in an `@implementation` block, use the Objective-C naming rules. For
code in a method of a C++ class, use the C++ naming rules.

For C functions and constants defined in a namespace, use C++ style, even if
most of the file is Objective-C.

`TEST` and `TEST_F` macros expand to C++ methods, so even if a unit test is
mostly testing Objective-C objects and methods, the test should be written using
C++ style.

## #import and #include in the `ios/` directory

#import directive can be used to import C++ and Objective-C headers for all
source code in the `ios/` directory. This differs from the Google Objective-C Style
Guide, which requires using #include directive for C++ headers.
