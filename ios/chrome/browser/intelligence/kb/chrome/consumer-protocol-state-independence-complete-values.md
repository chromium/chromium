---
name: ios/consumer-protocol-state-independence-complete-values
description: When pushing updates to a consumer, send self-contained current values rather than delta increments or relative state changes.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: State Independence
    quote: State independence means that each update sent over the consumer protocol should be self-contained and should not rely on the consumer's internal state being implicitly synchronized with a specific prior model state.
---

**Rule.** Consumer updates must be self-contained and convey full current values
rather than relative incremental deltas (e.g. `setReceivedBytes:`, not
`addReceivedBytes:`).

**Why.** State-independent updates allow the consumer to render correctly
without requiring knowledge of prior update history or replay ordering. See
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
