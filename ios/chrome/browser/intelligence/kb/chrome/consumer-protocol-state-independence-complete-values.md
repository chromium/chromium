---
name: ios/consumer-protocol-state-independence-complete-values
rubric: design
severity: should_fix
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
