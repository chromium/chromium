---
name: ios/consumer-data-source-only-for-interaction-queries
description: When pulling data from the mediator via consumer data sources, use them only for immediate, transient interaction decisions without sender parameters.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: Interaction-Time Queries
    quote: If the value is only used for an immediate, transient decision, the view controller may pull it from a consumer data source... Unlike a UIKit data source, a consumer data source method must not take the requesting consumer (self) as a sender parameter.
---

**Rule.** Consumer data sources must be restricted to immediate, transient
interaction-time queries (such as context menu actions) and must not take sender
`self` parameters.

**Why.** Consumer data sources should not become polling mechanisms. Persistent
state must be pushed via consumer protocols to preserve the single direction of
data flow. See `ios/architecture/mediators.md` ("Interaction-Time Queries").

## Examples

Bad:

```cpp
@protocol MenuConsumerDataSource <NSObject>
- (NSArray*)menuDataSource:(id)dataSource itemsForMenu:(UIMenu*)menu;
@end
```

Good:

```cpp
@protocol MenuConsumerDataSource <NSObject>
- (NSArray<MenuAction*>*)contextMenuActionsForIdentifier:(const base::Uuid&)uuid;
@end
```
