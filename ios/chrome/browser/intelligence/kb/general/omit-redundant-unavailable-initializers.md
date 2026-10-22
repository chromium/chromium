---
name: ios/omit-redundant-unavailable-initializers
rubric: readability
severity: should_fix
---

**Rule.** When defining Objective-C classes with custom designated initializers,
annotate each designated initializer taking required parameters with
`NS_DESIGNATED_INITIALIZER`, and explicitly mark all unsupported or superseded
superclass initializers as `NS_UNAVAILABLE`—including
`- (instancetype)init NS_UNAVAILABLE;` alongside `-initWithFrame:` and
`-initWithCoder:` on `UIView` subclasses. If convenience initializers are
provided, ensure they chain to the designated initializer rather than calling
super. Conversely, do not redundantly redeclare initializers or availability
macros in subclasses when the superclass (such as `ChromeCoordinator` with
`-initWithBaseViewController:browser:` or `-init NS_UNAVAILABLE`) has already
established them and the subclass does not add parameters or alter
initialization semantics, as Clang automatically inherits declarations and
propagates `NS_UNAVAILABLE` down the class hierarchy.

**Why.** Explicitly marking custom designated initializers with
`NS_DESIGNATED_INITIALIZER` enables Clang compiler verification to ensure
subclasses and convenience initializers chain correctly throughout the
inheritance hierarchy. Disabling unsupported superclass initializers like
`-initWithFrame:`, `-initWithCoder:`, and parameterless `-init` with
`NS_UNAVAILABLE` prevents callers from instantiating objects in an uninitialized
or invalid state (e.g., via `[[Class alloc] init]`). Conversely, redeclaring
identical initializer signatures or `NS_UNAVAILABLE` macros already present on a
superclass introduces redundant boilerplate and maintenance overhead, because
Clang already propagates availability attributes down the inheritance hierarchy.

## Examples

Bad:

```cpp
// Missing NS_DESIGNATED_INITIALIZER and leaves -init / UIView initializers available:
@interface GeminiConsentRowView : UIView
- (instancetype)initWithRow:(GeminiConsentRow*)row;
@end

// Redundantly redeclares initializers already inherited from ChromeCoordinator:
@interface QuickDeleteOtherDataCoordinator : ChromeCoordinator
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@end
```

Good:

```cpp
// Designated initializer marked; unsupported superclass initializers disabled:
@interface GeminiConsentRowView : UIView
- (instancetype)initWithRow:(GeminiConsentRow*)row NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
@end

// Inherits ChromeCoordinator initializers without redundant redeclaration:
@interface QuickDeleteOtherDataCoordinator : ChromeCoordinator
@end
```
