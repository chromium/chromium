---
name: ios/prefix-boolean-objective-c-methods-with-assertive-verbs
description: When declaring Objective-C BOOL methods, properties, or ivars, use assertive query prefixes rather than bare adjectives, nouns, trailing verbs, or lifecycle prefixes, and specify getter=is...
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
  - path: styleguide/objective-c/objective-c.md
    heading: Objective-C++ style matches the language
    quote: For code in an `@implementation` block, use the Objective-C naming rules. For code in a method of a C++ class, use the C++ naming rules.
---

**Rule.** When declaring or implementing Objective-C and Objective-C++ methods,
properties, or backing ivars that represent boolean queries, states, or
attributes:

1. **Methods returning `BOOL` (and custom getters)**: Prefix the selector name
   with an assertive or auxiliary verb (such as `is`, `has`, `can`, `should`, or
   `allows`). Do not use bare adjectives (e.g., `geminiEligible`,
   `readyForSync`, `profileValid`), bare nouns (e.g., `variationsStoredSeed`),
   imperative verbs (e.g., `showPlusButton`), trailing verbs (e.g.,
   `localEntitiesExist`), or lifecycle prefixes (`will...`, `did...`). Reserve
   `will...`/`did...` for lifecycle notifications, phrasing capability queries
   with predicates (e.g., `isDropAllowed:` instead of `willAllowDrop:`).
2. **Boolean properties and backing ivars**: For properties representing an
   adjective or state, omit `is` from the property and ivar name (e.g.,
   `@property BOOL usable;`, `_fuseboxEligible`) and explicitly declare
   `getter=is<Name>` (`getter=isUsable`), ensuring setters (`setUsable:`) and
   backing storage remain idiomatic.
3. **Exceptions and configuration flags**: Prefix the property name directly
   with `is` only when needed to avoid collision with C++ keywords (e.g.,
   `BOOL isVirtual`). For configuration toggles or option flags, prefix with a
   verb (e.g., `BOOL useHeadlineFont`) rather than a bare noun.

**Why.** Under Cocoa and Google Objective-C style conventions, boolean query
methods and accessors express assertions or questions. Prefixing selector names
with assertive auxiliary verbs (`is`, `has`, `can`, `should`, `allows`) makes
call sites read naturally as grammatical conditions (e.g.,
`if ([self isGeminiEligible])`, `if ([self hasLocalEntities])`) and clearly
distinguishes active queries from object accessors (which use bare nouns) or
mutating commands (which use imperative verbs). In contrast, placing verbs at
the end (e.g., `localEntitiesExist`) or using bare adjectives (`profileValid`)
obscures query semantics. For boolean properties, omitting `is` from the
property name and ivar maintains standard Cocoa setter and storage naming
(`setUsable:`, `_fuseboxEligible`) while `getter=is<Name>` provides fluent query
syntax without generating awkward setters like `setIsUsable:`. Finally, direct
`is` prefixes prevent compilation conflicts with reserved C++ keywords (such as
`virtual`), and verb prefixes on configuration flags distinguish options from
object instances.

## Examples

Bad:

```cpp
// Bare adjectives, nouns, imperative verbs, or trailing verbs for boolean methods:
- (BOOL)geminiEligible;  // Bare adjective; looks like an object accessor.
- (BOOL)localEntitiesExist;  // Trailing verb; should lead with auxiliary verb.
- (BOOL)showPlusButton;  // Imperative verb sounds like a mutating command.
- (BOOL)willAllowDrop:(id<UIDropSession>)session;  // Lifecycle prefix, not a query.

// Non-idiomatic property and ivar declarations:
@property(nonatomic, assign) BOOL editable;  // Missing assertive getter=isEditable.
@property(nonatomic, readonly) BOOL isUsable;  // Redundant 'is' prefix on property name.
@property(nonatomic, readonly) BOOL virtualCard;  // Repeats noun; 'virtual' is a C++ keyword.
@property(nonatomic, assign) BOOL headlineFont;  // Bare noun sounds like a font object.
BOOL _isFuseboxEligible;  // Ivar should not include 'is' prefix.
```

Good:

```cpp
// Assertive auxiliary verbs clearly convey boolean queries:
- (BOOL)isGeminiEligible;
- (BOOL)hasLocalEntities;
- (BOOL)shouldShowPlusButton;
- (BOOL)isDropAllowed:(id<UIDropSession>)session;

// Idiomatic property declarations, getters, and ivars:
@property(nonatomic, assign, getter=isEditable) BOOL editable;
@property(nonatomic, readonly, getter=isUsable) BOOL usable;
@property(nonatomic, readonly) BOOL isVirtual;  // Direct 'is' avoids C++ keyword conflict.
@property(nonatomic, assign) BOOL useHeadlineFont;  // Verb prefix clarifies config flag.
BOOL _fuseboxEligible;
- (void)setFuseboxEligible:(BOOL)eligible { _fuseboxEligible = eligible; }
```
