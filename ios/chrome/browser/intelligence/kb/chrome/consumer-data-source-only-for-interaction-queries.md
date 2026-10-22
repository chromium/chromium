---
name: ios/consumer-data-source-only-for-interaction-queries
rubric: design
severity: should_fix
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
