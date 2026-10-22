---
name: ios/consumer-protocol-identify-items-by-stable-id-not-index
rubric: safety
severity: should_fix
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
