---
name: ios/mediator-class-name-suffix-reserved
description: Reserve the *Mediator class name suffix exclusively for classes that mediate between the model layer and the UI layer, never for coordinator helpers or model services
rubric: readability
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: Architectural Overview
    quote: The class name suffix `*Mediator` is reserved exclusively for classes that mediate between the model layer and the UI layer as described in this document; it must not be used for general coordinator helpers or model-layer classes.
  - path: ios/architecture/README.md
    heading: Naming Convention
    quote: Consistent names make each object's role recognizable at a glance. Suffixes such as `Coordinator`, `Mediator`, `ViewController`, `Consumer`, `Mutator`, and `Commands` each identify a role in a pattern.
---

**Rule.** Reserve the `*Mediator` class name suffix exclusively for classes that
mediate between the model layer and the UI layer. Never use it for coordinator
helpers or model services.

**Why.** The `Mediator` suffix tells readers that a class sits between the model
layer and the UI layer; a coordinator helper or model service with that suffix
misstates its architectural role and blurs the layer boundary. See
`ios/architecture/mediators.md#architectural-overview` and
`ios/architecture/README.md#naming-convention`.

## Examples

Bad:

```cpp
// Bad 1: Model-layer keyed service misnamed with *Mediator.
class FooPersistenceMediator : public KeyedService {};

// Bad 2: Coordinator animation helper misnamed with *Mediator (does not mediate
// between the model layer and the UI layer).
@interface FooAnimationMediator : NSObject
- (void)animateTransitionInView:(UIView*)containerView;
@end
```

Good:

```cpp
// Good 1: Model-layer keyed service uses *Service.
class FooPersistenceService : public KeyedService {};

// Good 2: Coordinator animation helper uses *Helper; only the class that
// mediates between the model layer and the UI layer uses *Mediator.
@interface FooAnimationHelper : NSObject
- (void)animateTransitionInView:(UIView*)containerView;
@end

@protocol FooConsumer;
class WebStateList;

@interface FooMediator : NSObject

@property(nonatomic, weak) id<FooConsumer> consumer;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

@end
```
