---
name: ios/consumer-protocol-no-mediator-sender-parameter
rubric: design
severity: should_fix
---

**Rule.** Methods in consumer protocols must not take the mediator as a sender
parameter (e.g. `-mediator:didUpdateItems:`). Consumer protocols must be pure
decoupled data sinks.

**Why.** Passing the mediator couples the view controller to the concrete
mediator and mimics UIKit delegate patterns where decoupled data flow is
required. See `ios/architecture/mediators.md#consumer-protocols`.

## Examples

Bad:

```cpp
@protocol TabGridConsumer <NSObject>
- (void)tabGridMediator:(TabGridMediator*)mediator didUpdateTabs:(NSArray*)tabs;
@end
```

Good:

```cpp
@protocol TabGridConsumer <NSObject>
- (void)setTabItems:(NSArray<TabGridItem*>*)tabs;
@end
```
