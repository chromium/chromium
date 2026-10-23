---
name: ios/mediator-class-name-suffix-reserved
description: When creating mediator classes, use the Mediator suffix exclusively for classes that mediate between the model layer and the UI layer.
rubric: readability
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: Architectural Overview
    quote: The class name suffix *Mediator is reserved exclusively for classes that mediate between the model layer and the UI layer as described in this document; it must not be used for general coordinator helpers or model-layer classes.
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
