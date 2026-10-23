---
name: ios/document-objective-c-class-and-interface-declarations
description: When declaring Objective-C classes, category extensions, protocols, or methods, document purpose, contracts, nil fallbacks, and testing roles quoting symbols in backticks; avoid tautologies and placeholders
rubric: readability
severity: should_fix
citations:
  - path: docs/documentation_guidelines.md
    heading: Interface / class documentation
    quote: Non-trivial classes or sets of function declarations in header files should include comments with a brief description of the purpose of the class or set of functions as well as examples of expected usage.
  - path: docs/documentation_best_practices.md
    heading: Method and class comments
    quote: 'Class / Module API documentation: The header / Javadoc / docstring comments for a class or a whole file. This documentation gives a brief overview of what the class / file does'
  - path: styleguide/objective-c/objective-c.md
    heading: Disambiguating Symbols
    quote: Where needed to avoid ambiguity, use backticks to quote variable names and symbols in comments in preference to using quotation marks or naming the symbols inline.
  - path: styleguide/objective-c/objective-c.md
    heading: Nullability annotations in the `ios/` directory
    quote: Nullability annotations, such as `nonnull` and `nullable`, may not be used in the `ios/` directory. This differs from the Google Objective-C Style Guide, which permits these annotations.
  - path: docs/ios/style.md
    heading: TODO comments
    quote: Every TODO must have a bug
  - path: styleguide/objective-c/objective-c.md
    heading: Delineate interface implementations with `#pragma mark -`
    quote: To keep implementation files organized and navigable, method implementations should be clearly grouped by the interfaces they implement, and each such group should be marked with a `#pragma mark -`, followed by the interface name
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
  - path: docs/documentation_guidelines.md
    heading: Implementation documentation
    quote: Code should be as self documenting as possible, but sometimes that isn't enough. Maybe there's a tricky bit of implementation that isn't obvious.
---

**Rule.** Precede Objective-C class, class extension, category, protocol
(`@interface`, `@protocol`), method, property, and initializer declarations with
doc comments explaining their purpose, responsibilities, parameter semantics,
and contracts, quoting referenced parameter names, types, and symbols in
backticks (`symbol`), and documenting delegate or observer callback invocation
conditions ("Called when ..."). Do not omit doc comments even when a declaration
appears self-explanatory, and do not leave placeholder comments (such as
`// TODO: comment`). Begin class comments with a concise overview before
enumerating details, document concrete behaviors and managed data, and keep
protocol comments focused on the abstract role the conforming object plays
rather than naming a concrete conforming class or coupling the protocol to
specific subviews or granular method-level operations. When declaring designated
initializers (`NS_DESIGNATED_INITIALIZER`) or methods that accept parameters
that may be nil, state which parameters are optional and the default or fallback
behavior when they are nil. In Objective-C++ implementation (`.mm`) files,
precede every private method, internal initializer, or helper that is not
declared in an interface or protocol with a doc comment detailing its purpose,
preconditions, parameters, return value, side effects, and external state
dependencies (such as orientation, layout traits, or active presentation mode),
and place such helpers under `#pragma mark - Private` (never under a protocol or
superclass section). Conversely, do not repeat method-level doc comments on
implementations of protocol, delegate, observer-bridge, or superclass methods
grouped under `#pragma mark - <ProtocolName>`, and do not write comments that
merely describe trivial forwarding; the header or protocol declaration is the
authoritative contract, and implementation comments are reserved for non-obvious
internal mechanics.

**Why.** Public header files and declarations serve as the authoritative API
contract for clients and future maintainers; omitting comments on interfaces,
extensions, accessors, or initializers obscures why they exist, leaves parameter
expectations and default values ambiguous, and forces callers to inspect
implementation files. Describing specific subviews, granular operations, or a
concrete conforming class in protocol comments leaks implementation details into
abstract contracts and couples callers to a class they should only know through
the protocol, whereas keeping protocol comments high-level preserves clean
abstractions while method comments capture granular operations. Because
nullability annotations (`nonnull`/`nullable`) are disallowed in `//ios/`,
caller contracts must spell out nil handling and fallback behavior in prose.
Undeclared private helpers have no header declaration, so a doc comment under
`#pragma mark - Private` is the only place their parameter contracts and
external state dependencies are recorded, while repeating doc comments on
protocol or superclass method implementations clutters implementation files and
drifts as signatures evolve. Quoting symbols and parameters in backticks avoids
ambiguity and integrates cleanly with code search tools.

## Examples

Bad:

```cpp
// Bad: Placeholder TODO and unquoted symbols.
// Mediator for the Assistant AIM UI.
@interface AssistantAIMMediator : NSObject
// TODO: comment
@property(nonatomic, assign, readonly) BOOL collapsible;
@end

// Bad: Protocol coupled to concrete subviews and a concrete conformer; method
// comment restates the selector.
// Protocol for FacePileView to update its shareButton background color.
// Implemented by FacePileViewController.
@protocol FacePileColorUpdater <NSObject>
// Updates the face pile background color.
- (void)updateFacePileBackgroundColor:(UIColor*)color;
@end

// Bad: Undocumented nil fallback on the initializer.
@interface ConsentViewController : UIViewController
// Initializer for the consent view controller.
- (instancetype)initWithIsAccountManaged:(BOOL)isAccountManaged
                                 country:(NSString*)country
    NS_DESIGNATED_INITIALIZER;
@end

// In FeatureHandler.h:
@protocol FeatureHandlerDelegate
// Bad: Missing callback invocation trigger.
- (void)featureHandler:(FeatureHandler*)handler didCompleteWithResult:(BOOL)result;
@end

// In FeatureHandler.mm:
#pragma mark - FeatureHandlerDelegate

// Bad: Redundant doc comment duplicating the protocol declaration.
// Called when the feature handling sequence completes.
- (void)featureHandler:(FeatureHandler*)handler didCompleteWithResult:(BOOL)result {
  [self handleCompletionWithResult:result];
}

#pragma mark - IdentityManagerObserving

// Bad: Redundant doc comment on an observer bridge implementation.
// Called when a user changes the sign-in state.
- (void)primaryAccountDidChange:(const signin::PrimaryAccountChangeEvent&)event {
  [self updateUI];
}

#pragma mark - ChromeTableViewController

// Bad: Private helper misplaced under the superclass mark, undocumented.
- (void)updateLayoutWithAnimationDuration:(NSTimeInterval)duration {
  [self updateUIForScene:self.view.window.windowScene duration:duration];
}
```

Good:

```cpp
// Good: Class documented with purpose and managed data; symbols quoted in backticks.
// Mediator that manages the business logic and data for the AI mode Assistant.
@interface AssistantAIMMediator : NSObject
// Whether the menu expands or collapses when tapped.
@property(nonatomic, assign, readonly) BOOL collapsible;
@end

// Good: Protocol focused on the abstract role.
// Receives theme color updates for views that display a face pile.
@protocol FacePileColorUpdater <NSObject>
// Applies `color` as the background of the face pile.
- (void)updateFacePileBackgroundColor:(UIColor*)color;
@end

// Good: Optional nil parameter documented on the designated initializer.
@interface ConsentViewController : UIViewController
// Designated initializer for the consent view controller. `country` is
// optional; passing nil displays the default consent UI.
- (instancetype)initWithIsAccountManaged:(BOOL)isAccountManaged
                                 country:(NSString*)country
    NS_DESIGNATED_INITIALIZER;
@end

// In FeatureHandler.h:
// Delegate receiving lifecycle notifications from `FeatureHandler`.
@protocol FeatureHandlerDelegate
// Called when the feature handling sequence completes; `result` is YES when
// the feature was applied.
- (void)featureHandler:(FeatureHandler*)handler didCompleteWithResult:(BOOL)result;
@end

// In FeatureHandler.mm:
#pragma mark - FeatureHandlerDelegate

- (void)featureHandler:(FeatureHandler*)handler didCompleteWithResult:(BOOL)result {
  [self handleCompletionWithResult:result];
}

#pragma mark - IdentityManagerObserving

- (void)primaryAccountDidChange:(const signin::PrimaryAccountChangeEvent&)event {
  [self updateUI];
}

#pragma mark - Private

// Updates the layout for the current window scene orientation and presentation
// mode, animating view transitions over `duration`.
- (void)updateLayoutWithAnimationDuration:(NSTimeInterval)duration {
  [self updateUIForScene:self.view.window.windowScene duration:duration];
}
```
