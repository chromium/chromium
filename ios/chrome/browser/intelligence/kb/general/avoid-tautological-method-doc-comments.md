---
name: ios/avoid-tautological-method-doc-comments
description: When documenting methods, protocols, or helpers, describe purpose directly with active verbs; avoid tautological restatements, meta prefixes, concrete class coupling, or redundant parameter types
rubric: readability
severity: nit
citations:
  - path: docs/documentation_best_practices.md
    heading: Method and class comments
    quote: The header / Javadoc / docstring comments that say what methods do and how to use them. This documentation is the contract of how your code must behave.
  - path: styleguide/objective-c/objective-c.md
    heading: Delineate interface implementations with `#pragma mark -`
    quote: This kind of grouping should happen for the implementations's public methods, superclass methods, private methods, and each protocol the implementation conforms to.
---

**Rule.** When documenting methods, Objective-C protocols (such as consumers,
delegates, or mutators), members and properties, test fixtures, or helpers:

1. Describe the semantic purpose, action, configured state, or modeled scenario
   directly using concise, third-person active verb phrases (e.g.,
   `// Sets whether ...`, `// Sets the ...`). Avoid vague summaries (such as
   'log metrics') or meta-commentary prefixes (such as 'Private method to...' or
   'Helper function to...').
2. Avoid tautological restatements that merely echo the method or selector name
   (such as `// Returns the <name>` or `// Sets the <name>`) or, for ivars,
   members, and properties, the variable name or type (such as
   `// The model for passkeys.` above `_passkeyModel`).
3. Keep interface contracts decoupled from concrete implementers: on protocol
   declarations and method doc comments, do not name concrete caller or
   conforming classes (such as specific mediator, coordinator, or view
   controller classes).
4. Do not redundantly narrate parameter types (e.g., prefer
   `// Sets whether ...` over `// Sets the boolean on whether ...`, and avoid
   `// Sets the string for ...`) that are already explicit in the method
   signature.
5. Avoid enumerating brittle, evolving feature lists (such as listing
   `(Gemini, Lens)`); describe feature categories abstractly to prevent comments
   from becoming stale.

**Why.** Vague summaries, meta-commentary prefixes, and tautological selector
echoes add visual noise without clarifying API contracts, especially when
visibility is already apparent from scope or pragma marks. Coupling protocol and
method comments to concrete mediator, coordinator, or view controller classes
violates interface decoupling and causes doc drift when refactoring. Redundantly
narrating parameter types duplicates the method signature, while hardcoding
evolving feature lists creates brittle doc comments that quickly rot as features
evolve. Direct active verbs and abstract categories keep documentation concise,
robust, and evergreen.

## Examples

Bad:

```cpp
// Concrete mediator/view controller coupling on protocol declaration:
// Consumer for the QuickDeleteOtherDataMediator to set the
// QuickDeleteOtherDataViewController.
@protocol QuickDeleteOtherDataConsumer <NSObject>

// Redundant parameter type and concrete class coupling:
// Sets the boolean on whether the ViewController should show the cell.
- (void)setShouldShowMyActivityCell:(BOOL)shouldShowMyActivityCell;

// Meta-commentary prefix and vague summary:
// Helper method to log scan card metrics.
- (void)logScanCardAction:(ScanCardOfferToSaveAction)action;

// Brittle evolving feature list:
// Returns whether the AI entry points (Gemini, Lens) should be shown.
- (BOOL)shouldShowFeatureEntryPoints;
```

Good:

```cpp
// Abstract role decoupled from concrete classes:
// Consumer for the "Quick Delete Other Data" page.
@protocol QuickDeleteOtherDataConsumer <NSObject>

// Concise condition without redundant type or class coupling:
// Sets whether the "My Activity" cell should be shown.
- (void)setShouldShowMyActivityCell:(BOOL)shouldShowMyActivityCell;

// Direct active verb stating action and UI context:
// Logs the user action in scan card edit view.
- (void)logScanCardAction:(ScanCardOfferToSaveAction)action;

// Abstract category without hardcoded feature lists:
// Returns whether the AI entry points should be shown.
- (BOOL)shouldShowFeatureEntryPoints;
```
