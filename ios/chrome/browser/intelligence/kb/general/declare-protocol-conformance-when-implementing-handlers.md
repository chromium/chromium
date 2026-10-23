---
name: ios/declare-protocol-conformance-when-implementing-handlers
description: Declare internal Objective-C protocol conformance in private extensions under `#pragma mark`, implement or stub methods, and remove obsolete conformances, callbacks, and `#import`s
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Delineate interface implementations with `#pragma mark -`
    quote: This kind of grouping should happen for the implementations's public methods, superclass methods, private methods, and each protocol the implementation conforms to.
  - path: agents/prompts/common.md
    heading: Include Hygiene & IWYU
    quote: 'Cleanup Unused Includes: Remove unused includes when refactoring.'
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 09. Delete unused imports and build deps
    quote: Remove `#include`s, `#import`s, and forward declarations that are no longer needed.
---

**Rule.** When an Objective-C/Objective-C++ class adopts internal protocols not
required by external callers (such as command protocols, observer-bridge
`<Type>Observing` protocols, UIKit delegate or data-source protocols like
`UITextFieldDelegate`, or coordinator delegate protocols implemented by a parent
coordinator):

1. Declare conformance to `<ProtocolName>` in a private class extension
   (`@interface ClassName () <ProtocolName>`) within the implementation (`.mm`)
   file rather than the public header (`.h`), privately import the protocol
   header, and wire the handler target accordingly.
2. Group method implementations under `#pragma mark - <ProtocolName>`. Do not
   declare protocol conformance without implementing its methods; if
   implementation is deferred across multi-part CLs, provide stubs with
   attributed `// TODO(crbug.com/<id>):` comments or defer declaring conformance
   until needed.
3. When refactoring or dropping observer, delegate, listener, or handler
   functionality, remove obsolete protocol conformances from class interfaces
   and private extensions (`@interface ... <Protocol>`), delete orphaned
   callback implementations and associated `#pragma mark`s, and remove unused
   `#import` directives.

Consumer, consumer data source, and mutator conformances are not internal
details: they are the public API of the view controller or mediator, which the
coordinator wires together (`_mediator.consumer = _viewController;`,
`_viewController.mutator = _mediator;`). Declare them in the public header
(`@interface FooViewController : UIViewController <FooConsumer>`,
`@interface FooMediator : NSObject <FooDataSource, FooMutator>`), not in a class
extension.

**Why.** Declaring internal protocol conformance in public headers unnecessarily
widens the public interface, leaks implementation details, and forces consumers
of the header to transitively import protocol headers. Adopting internal
protocols in private class extensions preserves encapsulation and minimizes
compilation dependencies while still enabling Clang method signature
verification and `conformsToProtocol:` checks. By contrast, a consumer,
data-source, or mutator conformance is the contract the coordinator relies on
when connecting the view controller and the mediator, so hiding it in a class
extension would force the coordinator to cast or lose compile-time checking.
Delineating protocol methods under `#pragma mark - <ProtocolName>` keeps
implementation files navigable, and stubbing deferred methods with attributed
TODOs prevents incomplete conformances from obscuring event handling.
Conversely, retaining obsolete protocol conformances, orphaned callbacks, and
unused `#import` directives after refactoring or removing listener/delegate
functionality introduces unnecessary compile-time coupling, creates dead code,
and misleads maintainers about which events or state transitions the class
actually handles.

## Examples

Bad:

```cpp
// In FeatureHandler.h:
// Bad: Internal protocol declared in public header instead of private extension.
@interface FeatureHandler : NSObject <LayoutTransitionCoordinating>
@end

// In FeatureHandler.mm:
#import "ios/chrome/app/profile/profile_state_observer.h"

// Bad: Conformance declared without implementing methods; obsolete protocol retained.
@interface FeatureHandler () <ProfileStateObserver, OtherDataCommands>
@end

@implementation FeatureHandler

// Bad: Method implemented without grouping under #pragma mark - OtherDataCommands.
- (void)stopOtherDataPage { [_otherCoordinator stop]; }

// Bad: Orphaned callback and pragma retained after dropping UITextFieldDelegate.
#pragma mark - UITextFieldDelegate
- (BOOL)textFieldShouldReturn:(UITextField*)textField { return YES; }

@end

// In FeatureViewController.mm:
// Bad: Consumer conformance hidden in the class extension forces the
// coordinator to cast when assigning `_mediator.consumer`.
@interface FeatureViewController () <FeatureConsumer>
@end
```

Good:

```cpp
// In FeatureHandler.h:
// Good: Public header remains clean of internal protocols.
@interface FeatureHandler : NSObject
@end

// In FeatureHandler.mm:
// Good: Unused profile_state_observer.h import removed; internal protocols in private extension.
@interface FeatureHandler () <LayoutTransitionCoordinating, OtherDataCommands>
@end

@implementation FeatureHandler

#pragma mark - LayoutTransitionCoordinating

- (void)layoutTransitionDidComplete {
  // TODO(crbug.com/123456): Implement after adding consumer.
}

#pragma mark - OtherDataCommands

- (void)stopOtherDataPage {
  [_otherCoordinator stop];
}

// Good: Obsolete ProfileStateObserver and UITextFieldDelegate conformances,
// orphaned callbacks, pragmas, and unused imports are deleted.

@end

// In FeatureViewController.h:
// Good: Consumer conformance is public API and stays in the header.
@interface FeatureViewController : UIViewController <FeatureConsumer>
@end
```
