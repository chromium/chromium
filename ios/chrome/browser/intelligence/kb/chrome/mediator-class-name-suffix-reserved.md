---
name: ios/mediator-class-name-suffix-reserved
rubric: readability
severity: should_fix
---

**Rule.** The class name suffix `*Mediator` is reserved exclusively for classes
that mediate between model objects and UI consumer protocols. Do not use it for
coordinator helpers or model services.

**Why.** Consistent naming allows engineers and automated tools to identify the
architectural role and boundaries of each class at a glance. See
`ios/architecture/mediators.md#architectural-overview` and
`ios/architecture/README.md#naming-convention`.

## Examples

Bad:

```cpp
// Model-layer keyed service misnamed as mediator
class TabPersistenceMediator : public KeyedService {};
```

Good:

```cpp
// Model-layer keyed service
class TabPersistenceService : public KeyedService {};

// UI-mediating Objective-C class
@interface TabGridMediator : NSObject
```
