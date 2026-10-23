---
name: ios/consumer-protocol-no-mediator-sender-parameter
description: When declaring consumer protocol methods, pass pure values and do not pass the mediator as a sender parameter.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: Consumer Protocols
    quote: Unlike UIKit delegate and data source protocols, a consumer protocol is a decoupled interface. Consumer methods must never take the mediator as a UIKit-style sender parameter (for example, -readingListMediator:updateItems:).
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
