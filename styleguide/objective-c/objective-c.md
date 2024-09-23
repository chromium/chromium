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

## File names

Files should always be named using C++ style (`snake_case.mm`, not
`CamelCase.mm`), even if they contain Objective-C++.

As an exception, names of Apple system classes may retain their camel casing;
for example a file which defines a category could be named
`NSSomeSystemClass+my_category.h`.

## Code Formatting

Use `nil` for null pointers to Objective-C objects, and `nullptr` for C++
objects.

## Delineate interface implementations with `#pragma mark -`

To keep implementation files organized and navigable, method implementations
should be clearly grouped by the interfaces they implement, and each such group
should be marked with a `#pragma mark -`, followed by the interface name (see
examples below). This kind of grouping should happen for the implementations's
public methods, superclass methods, private methods, and each protocol the
implementation conforms to. In each group, methods should appear in the same
order they are defined in the corresponding interface declaration. (This is less
important for superclass methods for UIKit subclasses; it's not a big deal if
the ordering of `UIViewController` subclass methods doesn't match the UIKit
header).

```objective-c
@interface ExampleViewController : UIViewController<ExampleConsumer, 
                                                    UITableViewDelegate>

...
@end

@implementation ExampleViewController

- (instancetype)init {
  ...
}

#pragma mark - Public Properties

- (NSString*)stringProperty {
  ...
}

... // Other properties

#pragma mark - UIViewController

- (void)viewDidLoad {
  ...
}

... // Other superclass methods

#pragma mark - UITableViewDelegate

... // Protocol methods

#pragma mark - ExampleConsumer

... // Protocol methods

#pragma mark - Private methods

... // Private methods

```

Private methods can be grouped differently if it helps make the code more
readable; for example, private methods that are just helpers for methods in
specific protocols could be grouped under a `#pragma mark - <Protocol> helpers`,
directly after the protocol methods, instead of with the other private methods.

Remember that any method which isn't declared in an interface needs a full
method comment.

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

\#import directive can be used to import C++ and Objective-C headers for all
source code in the `ios/` directory. This differs from the Google Objective-C
Style Guide, which requires using #include directive for C++ headers.

## Disambiguating Symbols
Where needed to avoid ambiguity, use backticks to quote variable names and
symbols in comments in preference to using quotation marks or naming the symbols
inline.

This is more specific than the Google Objective-C Style Guide which allows pipes
or backticks.
