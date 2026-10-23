---
name: ios/match-pragma-mark-to-interface-name
description: In Objective-C and Objective-C++, group implementations using `#pragma mark - <InterfaceName>`, match method definitions to header declaration order, and consolidate helpers under `#pragma mark - Private`
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Delineate interface implementations with `#pragma mark -`
    quote: To keep implementation files organized and navigable, method implementations should be clearly grouped by the interfaces they implement, and each such group should be marked with a `#pragma mark -`, followed by the interface name (see examples below). This kind of grouping should happen for the implementations's public methods, superclass methods, private methods, and each protocol the implementation conforms to. In each group, methods should appear in the same order they are defined in the corresponding interface declaration.
---

**Rule.** In Objective-C and Objective-C++ files (`.h`, `.mm`), delineate method
implementations and properties into clearly grouped sections using standard
`#pragma mark - <Name>` headers rather than ad-hoc comment banners:

- **Interface and Superclass Grouping**: Delineate protocol implementations and
  superclass overrides under `#pragma mark - <InterfaceName>` matching the exact
  interface or superclass name (e.g., `#pragma mark - <ProtocolName>`,
  `#pragma mark - UIViewController`, `#pragma mark - ChromeCoordinator`). Avoid
  generic umbrella markers (such as `#pragma mark - Protocols` or
  `#pragma mark - Delegates`), and consolidate duplicate sections for the same
  interface into a single section. Restrict each section strictly to methods
  declared by that interface or protocol, avoiding undeclared methods, foreign
  callbacks, or subdivided marks (e.g., avoid
  `#pragma mark - <ProtocolName> (<Subgroup>)`).
- **Method Ordering and Clean Contracts**: Within each interface, protocol, or
  public section, define method implementations in the same relative order as
  they are declared in the corresponding interface (`@interface`) or protocol
  (`@protocol`) header (`.h`) (though strict ordering is relaxed for system
  UIKit subclass overrides). When adding, reordering, or updating methods in
  either the header or implementation, update the counterpart file so
  declarations and definitions match. Omit redundant doc comments on protocol
  methods in implementation files, relying on the interface header as the
  authoritative contract. Keep active interface methods under their matching
  interface rather than moving them to `#pragma mark - Unused`.
- **Properties, Public, and Lifecycle Grouping**: Place public methods under
  `#pragma mark - Public` and custom property accessors under
  `#pragma mark - Properties` (or `#pragma mark - Public Properties`) rather
  than interspersing them among lifecycle methods. Group class and lifecycle
  methods (such as coordinator `-start`/`-stop` or view coordinator lifecycle
  overrides) together in their appropriate section; never place
  `#pragma mark - <ProtocolName>` dividers between related lifecycle methods.
- **Private Section Consolidation**: Consolidate internal private helper
  routines and private initializers into a single dedicated
  `#pragma mark - Private` section towards the bottom of the `@implementation`
  block (or `#pragma mark - <ProtocolName> helpers` immediately following
  protocol methods), and place public initializers under
  `#pragma mark - Initialization`. Do not place private helpers before
  initializers, do not create redundant duplicate `#pragma mark - Private`
  sections, and never place private helpers or private initializers inside
  `#pragma mark - Public` or superclass overrides under
  `#pragma mark - Private`. For category headings, use space-separated words
  (e.g., `#pragma mark - Private Helpers`), never concatenated CamelCase words
  (e.g., `#pragma mark - PrivateHelpers`).

**Why.** Per Chromium's Objective-C style guide ('Delineate interface
implementations with `#pragma mark -`'), method implementations must be clearly
grouped by the interfaces they implement, and within each group methods should
appear in the same order they are defined in the corresponding interface
declaration. Matching exact interface and superclass names without generic
umbrella markers immediately identifies method provenance and contracts.
Preserving declaration order between header declarations and implementation
definitions ensures consistent, predictable navigation across conforming classes
and source files, simplifies visual comparison during code review, and prevents
methods from being arbitrarily scattered. Subdividing protocol marks,
intermixing private helpers into protocol sections, placing superclass overrides
under Private, or inserting protocol dividers between related lifecycle methods
(such as `-start` and `-stop`) fragments class structure, breaks symbol
navigation, and misleads readers about interface conformance. Consolidating
private helpers under `#pragma mark - Private` with space-separated headings
eliminates visual noise and avoids fragmented duplicate sections.

## Examples

Bad:

```cpp
// In header (save_card_consumer.h), SaveCardConsumer declares:
// - (void)showConfirmation;
// - (void)setMessages:(NSArray*)messages;

@implementation PaymentsViewController

// Bad: Private helper before init and concatenated CamelCase heading.
#pragma mark - PrivateHelpers
- (void)logExitReason:(ExitReason)reason { ... }

#pragma mark - Initialization
- (instancetype)initWithConfig:(Config*)config { ... }
- (instancetype)initWithColors:(NSArray*)colors { ... }  // Bad: Private init in Initialization/Public.

#pragma mark - Public
- (void)reloadData { ... }
// Bad: Protocol divider placed between related public methods.
#pragma mark - PromoStyleViewControllerDelegate
- (void)focusFirstField { ... }

// Bad: Generic umbrella marker, subdivided mark, and undeclared method.
#pragma mark - PromoStyleViewControllerDelegate (Disclaimer)
- (void)didTapURLInDisclaimer:(NSURL*)URL { ... }
- (void)updateColors { ... }  // Bad: Undeclared helper in protocol section.

#pragma mark - Protocols  // Bad: Generic umbrella pragma mark.
// Bad: Method definition order inverted from header declaration order.
- (void)setMessages:(NSArray*)messages { ... }
- (void)showConfirmation { ... }

#pragma mark - Private
// Bad: Superclass override placed under Private instead of UIViewController mark.
- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
}

#pragma mark - Private  // Bad: Duplicate Private section.
- (void)cleanUp { ... }

@end
```

Good:

```cpp
// In header (save_card_consumer.h), SaveCardConsumer declares:
// - (void)showConfirmation;
// - (void)setMessages:(NSArray*)messages;

@implementation PaymentsViewController

#pragma mark - Initialization

- (instancetype)initWithConfig:(Config*)config { ... }

#pragma mark - Properties

- (void)setInGroup:(BOOL)inGroup { _inGroup = inGroup; }

#pragma mark - Public

- (void)reloadData { ... }
- (void)focusFirstField { ... }

#pragma mark - UIViewController

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapURLInDisclaimer:(NSURL*)URL { ... }

#pragma mark - SaveCardConsumer

// Good: Method definition order matches header declaration order.
- (void)showConfirmation { ... }
- (void)setMessages:(NSArray*)messages { ... }

#pragma mark - Private

- (instancetype)initWithColors:(NSArray*)colors { ... }
- (void)logExitReason:(ExitReason)reason { ... }
- (void)updateColors { ... }
- (void)cleanUp { ... }

@end
```
