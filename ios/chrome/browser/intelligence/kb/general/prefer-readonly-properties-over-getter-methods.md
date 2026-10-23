---
name: ios/prefer-readonly-properties-over-getter-methods
description: When exposing or mutating object state in Objective-C interfaces, declare @property accessors instead of standalone getter/setter methods and rely on compiler synthesis
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
---

**Rule.** In Objective-C and Objective-C++ class interfaces, categories, and
non-delegate protocol declarations (such as `*Consumer`, `*DataSource`, and
`*Mutator` protocols), expose and mutate object state or attributes using
`@property` declarations with appropriate atomicity and memory management
attributes (such as `nonatomic`, `readonly`, `copy`, `weak`, or `assign`) rather
than declaring standalone parameterless getter methods (e.g., `- (Type)foo` or
`- (BOOL)isFoo`), setter methods (e.g., `- (void)setFoo:(Type)foo`), or manual
accessor method pairs (hand-written accessors in the implementation are covered
by `ios/synthesize-protocol-properties-over-trivial-getters`). For read-only
state or category attributes accessed via dot syntax, declare
`@property(nonatomic, readonly)` (specifying `copy` for types with mutable
counterparts such as `NSString`). Reserve standalone method declarations for
actions, mutations taking multiple parameters, or operations with non-trivial
side effects or heavy computation. Three cases are excluded from this rule:

- Delegate protocols (`*Delegate`) never declare `@property`s; every delegate
  method passes the delegating object as its first parameter, so queries are
  methods such as `- (BOOL)fooShouldBar:(Foo*)foo`.
- A property exposing an unowned C++ pointer is declared with a bare `T*`
  (`@property(nonatomic, assign) PrefService* prefService;`); its required
  explicit accessors backed by a `raw_ptr<T>` or `base::WeakPtr<T>` ivar are
  covered by `ios/synthesize-protocol-properties-over-trivial-getters`.
- Private internal state is held in plain ivars declared in `@implementation`,
  not in private `@property`s added to a class extension.

**Why.** Declaring `@property` attributes explicitly communicates ownership,
mutability, and atomicity contracts to callers while enabling idiomatic
dot-syntax access, compiler-enforced immutability, and automatic compiler
synthesis of accessors and backing instance variables. Standalone getter/setter
methods and redundant custom getter implementations create unnecessary
boilerplate, increase maintenance overhead, obscure memory management semantics,
and violate Google Objective-C style guide conventions. The exclusions follow
the same reasoning: a property getter cannot receive the delegating object that
delegate methods must pass, a `raw_ptr<T>` ivar cannot be auto-synthesized
behind a bare `T*` property, and synthesized accessors for private state only
bloat binary size.

## Examples

Bad:

```cpp
// Bad: Standalone getter/setter methods in interfaces/categories
@interface CardViewController : UIViewController
- (NSString*)cardholderName;
- (void)setViewportFitCover:(BOOL)cover;
- (BOOL)isViewportFitCover;
@end

@interface NSString (Chromium)
- (std::string)cr_utf8;
@end

// Bad: Redundant manual getter in implementation
- (NSString*)headerTitle {
  return _headerTitle.length > 0 ? _headerTitle : @"";
}
```

Good:

```cpp
// Good: Declared properties with compiler synthesis
@interface CardViewController : UIViewController
@property(nonatomic, copy, readonly) NSString* cardholderName;
@property(nonatomic, assign) BOOL viewportFitCover;
@property(nonatomic, copy, readonly) NSString* headerTitle;
@end

@interface NSString (Chromium)
@property(nonatomic, readonly) std::string cr_UTF8String;
@end

// Implementation: omit manual getter; Clang auto-synthesizes accessors.

// Good: Excluded. Delegate protocols use query methods that pass the
// delegating object, never @property.
@protocol CardViewControllerDelegate <NSObject>
- (BOOL)cardViewControllerShouldShowCardholderName:
    (CardViewController*)cardViewController;
@end

```
