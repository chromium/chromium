---
name: ios/consumer-protocol-state-independence-complete-values
description: Pass complete current values for each updated consumer property rather than relative deltas.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: State Independence
    quote: State independence means that each update sent over the consumer protocol should be self-contained and should not rely on the consumer's internal state being implicitly synchronized with a specific prior model state.
---

**Rule.** Consumer methods that update a property must pass its complete current
value rather than a relative delta (for example, `setReceivedBytes:`, not
`addReceivedBytes:`).

**Why.** Self-contained updates ensure the consumer does not rely on being
implicitly synchronized with prior model states. See
`ios/architecture/mediators.md#state-independence`.

## Examples

Bad:

```cpp
@protocol DownloadConsumer <NSObject>
- (void)addReceivedBytes:(int64_t)bytesDelta;
@end
```

Good:

```cpp
@protocol DownloadConsumer <NSObject>
- (void)setReceivedBytes:(int64_t)totalBytes;
@end
```
