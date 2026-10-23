---
name: net/device_bound_sessions/avoid-priority-inversion-in-key-restore
rubric: correctness
severity: should_fix
---

**Rule.** When a CL changes the priority passed to `RestoreSessionKey()`, the
`SessionStore` restore methods (`RestoreSessionBindingKey()`,
`RestoreSessionAttestationKey()`) or the `net::RequestPriority` of a DBSC
`URLFetcher`, check for priority inversion. Keep the `kUserVisible` floor
enforced in one place inside `RestoreSessionKey()`, or raise the priority when a
higher-priority request joins in-flight best-effort work.

**Why.** This rule prevents priority inversion. Restoring a key can mean a slow
TPM unwrap, and DBSC shares that work: deferred requests join an in-flight
refresh, and the key service runs duplicate unwraps once, at the first caller's
priority. If best-effort work such as a startup prewarm starts the unwrap, a
user-visible navigation that joins it waits at best-effort priority. Enforcing
the floor in one place keeps new callers from reintroducing the inversion.

## Examples

Bad:

```cpp
// A prewarm path passes a lower priority straight through.
RestoreSessionKey(session_key, std::move(on_access_callback),
                  unexportable_keys::BackgroundTaskPriority::kBestEffort,
                  std::move(callback));
```

Good:

```cpp
void SessionServiceImpl::RestoreSessionKey(..., BackgroundTaskPriority priority,
                                           ...) {
  // Never unwrap below `kUserVisible`: the key service coalesces concurrent
  // unwraps at the first caller's priority, and a deferred request may join
  // this restore.
  const unexportable_keys::BackgroundTaskPriority unwrap_priority = std::max(
      priority, unexportable_keys::BackgroundTaskPriority::kUserVisible);
  ...
}
```
