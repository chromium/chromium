---
name: ios/avoid-mutable-setters-for-lifecycle-configuration
description: Inject lifecycle configuration and mandatory dependencies via designated initializers with CHECK, mark unconfigured initializers NS_UNAVAILABLE, and specify properties readonly instead of exposing mutable setters
rubric: correctness
severity: should_fix
citations:
  - path: docs/patterns/builder-and-parameter-bundle.md
    heading: The Builder & Parameter Bundle Patterns
    quote: The builder and parameter bundle patterns allow for giving a class a lot of configuration options at compile time, while leaving the runtime API small and the runtime constraints clear.
  - path: styleguide/objective-c/objective-c.md
    heading: Nullability annotations in the `ios/` directory
    quote: Nullability annotations, such as `nonnull` and `nullable`, may not be used in the `ios/` directory. This differs from the Google Objective-C Style Guide, which permits these annotations.
  - path: styleguide/c++/checks.md
    heading: CHECK(), DCHECK() and NOTREACHED()
    quote: CHECK(), DCHECK() and NOTREACHED() are all used to ensure that invariants hold. They document (and verify) programmer expectations that either some statement *always* holds true at the point of (D)CHECKing or that a piece of code is unreachable (for NOTREACHED).
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
---

**Rule.** Do not expose public C++ setters (`SetFooEnabled(bool)`), readwrite
properties, or post-construction property mutations (`item.property = ...`) for
value-type configuration—operating modes, flags, identifiers, metric values, or
view configuration—that is fixed for the object's lifetime and never reassigned.
Pass such lifecycle invariants directly into the constructor or designated
initializer, using dedicated initializers or a parameter bundle when callers
require distinct parameter subsets. Additionally:

- Pass scalar and enum configuration whose zero value (`0`) maps to a valid case
  through the initializer as well, so that an omitted assignment cannot silently
  select a valid-looking default.
- This rule does not apply to the documented property-injection seams:
  configuration properties a parent sets on a child coordinator between
  initialization and `-start`, and the weak `consumer`, `mutator`, `dataSource`,
  and `<name>Handler` properties through which coordinators wire mediators, view
  controllers, and command handlers.

**Why.** Exposing mutable setters for configuration that is fixed for an
object's lifetime introduces temporal coupling and two-phase initialization: the
object can exist in an unconfigured or semi-initialized state, a setter invoked
after the value has already been consumed (for example after a view is loaded)
fails silently, and every reader must defensively handle the unconfigured state.
Because C++ and Objective-C zero-initialize unassigned fields, an omitted scalar
or enum assignment silently selects `0`/`NO`, which is often a valid domain
variant, so the mistake passes runtime checks. Passing the configuration into
the constructor or designated initializer makes instantiation atomic and lets
the compiler enforce that every caller supplies it. The property-injection seams
are different: the coordinator documentation specifies that a parent configures
a child coordinator's properties before `-start`, and the mediator and command
documentation wires mediators, view controllers, and command handlers through
weak `consumer`, `mutator`, `dataSource`, and `<name>Handler` properties, so
those readwrite properties are the sanctioned mechanism rather than an
anti-pattern.

## Examples

Bad:

```cpp
// C++: a setter lets the object exist unconfigured until SetMode() runs, and
// the zero-valued enumerator is a valid mode, so an omitted call passes.
class FooController {
 public:
  void SetMode(FooMode mode) { mode_ = mode; }

 private:
  FooMode mode_ = FooMode::kStandard;
};

// Objective-C: value-type configuration mutated after construction.
@interface FooItem : NSObject
@property(nonatomic, assign) FooMetric metric;
@end

FooItem* item = [[FooItem alloc] init];
item.metric = FooMetric::kEnterprise;
```

Good:

```cpp
// C++: the mode is a constructor argument and never reassigned.
class FooController {
 public:
  explicit FooController(FooMode mode) : mode_(mode) {}

 private:
  const FooMode mode_;
};

// Objective-C: configuration is passed to the designated initializer.
@interface FooItem : NSObject
@property(nonatomic, readonly) FooMetric metric;
- (instancetype)initWithMetric:(FooMetric)metric NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@end

FooItem* item = [[FooItem alloc] initWithMetric:FooMetric::kEnterprise];

// Sanctioned injection seams, not flagged by this rule: a parent configures
// its child coordinator before -start, and weak consumer/mutator/handler
// properties wire the mediator, view controller, and command handlers.
@interface FooViewController : UIViewController <FooConsumer>
@property(nonatomic, weak) id<FooMutator> mutator;
@property(nonatomic, weak) id<FooCommands> fooHandler;
@end
```
