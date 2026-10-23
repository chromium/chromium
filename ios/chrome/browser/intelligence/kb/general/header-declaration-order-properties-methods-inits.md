---
name: ios/header-declaration-order-properties-methods-inits
description: In Objective-C header files, order interface declarations with properties first, followed by class methods, initializers, and instance methods
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Headers, Imports, and Declaration Order
    quote: 'Inside every `@interface` in a header file, order declarations as mandated by the Google Objective-C Style Guide: 1. `@property` declarations 2. Class methods (convenience constructors first) 3. Initializers (`-init...`) 4. Instance methods.'
---

**Rule.** In Objective-C and Objective-C++ header files (@interface
declarations), order member declarations in standard sequence: properties first,
followed by class methods (convenience constructors first), initializers, and
instance methods. Do not place property declarations after initializers or
instance methods.

**Why.** Consistent declaration ordering across header files creates a
predictable, uniform interface structure that makes class APIs and properties
easy to scan during navigation and code review. See `ios/best_practices.md`
("Headers, Imports, and Declaration Order").

## Examples

Bad:

```cpp
@interface AutofillAISaveEntityMediator : NSObject

- (instancetype)initWithParams:(autofill::SaveEntityParams)params
    NS_DESIGNATED_INITIALIZER;

- (void)acceptSaving;

@property(nonatomic, weak) id<AutofillAISaveEntityConsumer> consumer;

@end
```

Good:

```cpp
@interface AutofillAISaveEntityMediator : NSObject

@property(nonatomic, weak) id<AutofillAISaveEntityConsumer> consumer;

- (instancetype)initWithParams:(autofill::SaveEntityParams)params
    NS_DESIGNATED_INITIALIZER;

- (void)acceptSaving;

@end
```
