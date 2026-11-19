---
name: ios/consumer-protocol-identify-items-by-stable-id-not-index
description: When updating collections over a consumer protocol, identify items by a stable identifier rather than raw integer indices or index paths.
rubric: safety
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: State Independence
    quote: For the same reason, consumer updates identify items in a collection by stable identifiers, not by position. An index identifies the right item only if the consumer's items exactly match the model's at the moment of the update.
---

**Rule.** Consumer methods that update or remove collection items must not
identify targets by raw positional indices or `NSIndexPath` values. Pass a
stable identifier (such as `base::Uuid` or `web::WebStateID`) instead of a
positional index.

**Why.** A positional index identifies the right item only if the consumer's
collection exactly matches the model's state at the moment of the update. Stable
identifiers keep updates state-independent by identifying the target item
regardless of the consumer's prior state. See
`ios/architecture/mediators.md#state-independence`.

## Examples

Bad:

```cpp
@protocol ItemListConsumer <NSObject>
- (void)removeItemAtIndex:(NSUInteger)index;
- (void)reloadItemAtIndexPath:(NSIndexPath*)indexPath;
@end
```

Good:

```cpp
@protocol ItemListConsumer <NSObject>
- (void)removeItemWithIdentifier:(const base::Uuid&)identifier;
@end
```
