---
name: ios/omit-empty-objective-c-class-extensions
description: In Objective-C implementation files, declare ivars in @implementation, omit empty class extensions and empty ivar braces, and avoid declaring private methods in extensions
rubric: readability
severity: nit
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
  - path: styleguide/objective-c/objective-c.md
    heading: Delineate interface implementations with `#pragma mark -`
    quote: Remember that any method which isn't declared in an interface needs a full method comment.
---

**Rule.** In Objective-C and Objective-C++ implementation files (`.mm`),
structure implementation blocks cleanly and avoid redundant declarations:

1. Reserve class extensions (`@interface ClassName ()`) for private protocol
   conformances (`<ProtocolName>`, such as command or observer protocols) and
   the rare private `@property` that needs property semantics (for example, a
   custom setter with side effects or a readwrite redeclaration of a public
   readonly property). Private state belongs in `@implementation` ivars, not in
   private `@property` wrappers.
2. Do not declare private helper methods in class extensions; implement them
   directly in the `@implementation ClassName` block (optionally grouped under
   `#pragma mark - Private`).
3. Declare private instance variables directly inside the
   `@implementation ClassName { ... }` block rather than in a class extension
   (`@interface ClassName () { ... }`).
4. Omit empty class extensions (`@interface ClassName () \n @end`) entirely when
   no private protocol conformances (or such rare properties) are needed; begin
   the file directly with `@implementation ClassName`.
5. Omit empty instance variable braces (`{}`) after `@implementation ClassName`
   when declaring no instance variables.

**Why.** In modern Objective-C, methods defined in an `@implementation` block do
not require forward declarations to be callable within the same source file, and
instance variables can be declared directly in the implementation block.
Retaining empty class extensions, defining extensions solely to host ivars or
forward-declare private helper methods, or leaving empty `{}` ivar braces after
`@implementation` introduces unnecessary syntactic noise, boilerplate, and
visual clutter without providing encapsulation or compiler benefits. Following
the Google Objective-C style guide, ivars belong directly in `@implementation`
and class extensions should be omitted unless needed for private protocol
conformances (or the rare property that needs custom semantics); private
`@property` wrappers around internal state only add synthesized accessors that
bloat binary size.

## Examples

Bad:

```cpp
// Declaring private helper methods in a class extension:
@interface PaymentsViewController () <ItemDelegate>
- (void)logAction:(ActionType)action;
@end

// Defining a class extension solely to host ivars:
@interface PromoViewController () {
  AlertController* _alertScreen;
}
@end

// Private @property wrapper around plain internal state:
@interface FooMediator ()
@property(nonatomic, assign) BOOL loading;
@end

// Retaining empty ivar braces after @implementation:
@implementation BookmarkMediator {}
@end
```

Good:

```cpp
// Implement private methods directly without forward-declaring in the extension:
@interface PaymentsViewController () <ItemDelegate>
@end

// Declare ivars directly in @implementation and omit empty class extensions:
@implementation PromoViewController {
  AlertController* _alertScreen;
}
@end

// Keep plain internal state as an ivar; no class extension needed:
@implementation FooMediator {
  BOOL _loading;
}
@end

// Omit ivar braces entirely when no ivars are declared:
@implementation BookmarkMediator
@end
```
