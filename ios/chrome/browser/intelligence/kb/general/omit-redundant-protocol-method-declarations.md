---
name: ios/omit-redundant-protocol-method-declarations
description: In Objective-C interfaces and implementations, omit redundant method declarations or protocol conformances inherited from superclasses or headers, and consolidate duplicate methods when adopting protocols
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
---

**Rule.** In Objective-C class interfaces (`@interface`), class extensions
(`@interface ClassName ()`), and implementations, do not redundantly redeclare
methods or protocol conformances already provided by adopted protocols, public
headers, or superclasses, and do not introduce duplicate methods when conforming
to protocols:

1. **Redundant method declarations**: Do not redeclare methods in a class
   interface or class extension if they are already declared by an adopted
   protocol (`<ProtocolName>`) or inherited from a superclass (or a protocol
   adopted by the superclass); subclasses and conforming classes already inherit
   and expose those method declarations. Only redeclare properties if needed for
   compiler auto-synthesis or mutability refinement (e.g., `readonly` to
   `readwrite`).
2. **Redundant protocol conformances**: Do not redundantly declare protocol
   conformance in a class extension (`@interface Foo () <BarProtocol>`) or
   subclass interface when the class already adopts that protocol in its public
   header (`@interface Foo : Superclass <BarProtocol>`) or inherits it from an
   ancestor superclass. Remove duplicate protocol declarations from class
   extensions when the protocol is adopted by the public header or superclass.
3. **Consolidate duplicate methods upon protocol conformance**: When adapting a
   class to conform to a protocol, presenter, handler, or delegate, reuse or
   consolidate existing methods instead of adding redundant duplicate
   implementations that perform the same task. Adopt the protocol method
   signature directly or centralize shared logic in the conforming method rather
   than retaining parallel legacy helper methods.

**Why.** In Objective-C, subclasses and class extensions automatically inherit
all method declarations and protocol conformances from their superclasses and
public interfaces, while conforming classes inherit all method declarations from
adopted protocols. Redeclaring inherited or protocol methods in class interfaces
adds unnecessary boilerplate, clutters public headers, and risks signature or
documentation drift during refactoring. Similarly, repeating protocol names
already adopted by a public interface or superclass introduces visual noise and
redundant boilerplate without providing compiler or runtime benefits.
Furthermore, introducing duplicate concrete methods alongside protocol methods
clutters class APIs and creates parallel code paths that can diverge or
introduce maintenance bugs. Consolidating overlapping logic directly into the
protocol-conforming implementation keeps class contracts minimal and ensures
presentation or handling logic remains centralized in a single place.

## Examples

Bad:

```cpp
// Redundant method redeclarations already in TabPickerSnackbarPresenter or superclass:
@interface ComposeboxSnackbarPresenter : NSObject <TabPickerSnackbarPresenter>
- (void)showCannotReloadTabError;
- (void)showSnackbarForTabAttachmentLimit:(NSUInteger)attachmentLimit;
@end

@interface IncognitoGridMediator : BaseGridMediator
- (void)closeAllItems;  // Already declared in BaseGridMediator.
@end

// Class extension re-declaring a protocol already adopted publicly in
// foo_view_controller.h (@interface FooViewController : UIViewController <FooConsumer>):
@interface FooViewController () <FooConsumer, UITextFieldDelegate>
@end

// Duplicate method implementation alongside new protocol method:
@implementation ComposeboxSnackbarPresenter
- (void)showSnackbarForTabAttachmentLimitReachedWithCount:(NSUInteger)count {
  [self showSnackbarForTabAttachmentLimit:count];
}
- (void)showSnackbarForTabAttachmentLimit:(NSUInteger)attachmentLimit {
  [self.snackbarHandler showSnackbarMessage:[self makeMessageForLimit:attachmentLimit]];
}
@end
```

Good:

```cpp
// Protocol and superclass method declarations are inherited directly:
@interface ComposeboxSnackbarPresenter : NSObject <TabPickerSnackbarPresenter>
@end

@interface IncognitoGridMediator : BaseGridMediator
@end

// Omit FooConsumer because it is already adopted in foo_view_controller.h:
@interface FooViewController () <UITextFieldDelegate>
@end

// Consolidate logic into a single method conforming to the protocol:
@implementation ComposeboxSnackbarPresenter
#pragma mark - TabPickerSnackbarPresenter

- (void)showSnackbarForTabAttachmentLimit:(NSUInteger)attachmentLimit {
  [self.snackbarHandler showSnackbarMessage:[self makeMessageForLimit:attachmentLimit]];
}
@end
```
