---
name: ios/header-declaration-order-properties-methods-inits
description: Order declarations in Objective-C header @interface blocks as properties, class methods, initializers, then instance methods
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Headers, Imports, and Declaration Order
    quote: 'Inside every `@interface` in a header file, order declarations as mandated by the Google Objective-C Style Guide: 1. `@property` declarations 2. Class methods (convenience constructors first) 3. Initializers (`-init...`) 4. Instance methods'
---

**Rule.** Inside every `@interface` in an Objective-C or Objective-C++ header,
order declarations as `@property` declarations first, then class methods
(convenience constructors first), initializers (`-init...`), and instance
methods. Never declare a property after an initializer or instance method.

**Why.** The Google Objective-C Style Guide mandates this declaration order, so
every header puts a class's properties, construction, and behavior in the same
place, which makes `@interface` declarations fast to scan during navigation and
code review. See `ios/best_practices.md#headers-imports-and-declaration-order`.

## Examples

Bad:

```cpp
@protocol FooConsumer;
class FooService;

@interface FooMediator : NSObject

- (instancetype)initWithFooService:(FooService*)fooService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

+ (BOOL)isFeatureAvailable;

@property(nonatomic, weak) id<FooConsumer> consumer;

@end
```

Good:

```cpp
@protocol FooConsumer;
class FooService;

@interface FooMediator : NSObject

@property(nonatomic, weak) id<FooConsumer> consumer;

+ (BOOL)isFeatureAvailable;

- (instancetype)initWithFooService:(FooService*)fooService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

@end
```
