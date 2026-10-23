---
name: ios/consumer-protocol-identify-items-by-stable-id-not-index
description: When updating collections over a consumer protocol, identify items by stable identifiers rather than integer indices or row positions.
rubric: safety
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: State Independence
    quote: For the same reason, consumer updates identify items in a collection by stable identifiers, not by position. An index identifies the right item only if the consumer's items exactly match the model's at the moment of the update.
---

**Rule.** Consumer methods that update or remove collection items must identify
targets using stable identifiers (e.g. `base::Uuid` or unique IDs) rather than
array indices.

**Why.** Positional indices become stale during concurrent animations or rapid
updates, causing mutations to apply to the wrong items or crashing with
out-of-bounds errors. See `ios/architecture/mediators.md#state-independence`.

## Examples

Bad:

```cpp
@protocol ItemListConsumer <NSObject>
- (void)removeItemAtIndex:(NSUInteger)index;
@end
```

Good:

```cpp
@protocol ItemListConsumer <NSObject>
- (void)removeItemWithIdentifier:(const base::Uuid&)identifier;
@end
```
