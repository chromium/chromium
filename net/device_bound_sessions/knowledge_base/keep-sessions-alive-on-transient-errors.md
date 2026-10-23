---
name: net/device_bound_sessions/keep-sessions-alive-on-transient-errors
rubric: correctness
severity: should_fix
---

**Rule.** When error handling for refresh, registration or prewarming changes,
check that transient failures (`kTransientHttpError`, network or proxy errors,
and key-service errors for which `unexportable_keys::IsPersistentError()` is
false) neither terminate the session nor abort work that spans several sessions,
such as prewarming. Release waiting requests with a non-fatal result. Retries
should be bounded and use a small randomized delay, and the retry decision
should come from one named predicate rather than attempt-count checks spread
across functions.

**Why.** Ending a session on a transient failure, such as a server outage or a
dropped connection, forces the site to register again and discards its bound
credentials, and one session's transient error shouldn't stop work for other
sessions. Unbounded or synchronized retries add load to a server that is already
failing; a bounded, randomized retry decided in one place avoids that and stays
easy to audit.

## Examples

Bad:

```cpp
for (const SessionKey& key : sessions_to_prewarm) {
  if (Refresh(key) == SessionError::kTransientHttpError) {
    return;  // One flaky server stops prewarming for every other session.
  }
}
```

Good:

```cpp
const bool is_persistent_error =
    unexportable_keys::IsPersistentError(key_id_or_error.error());
// Only persistent failures end the session; transient ones release the
// waiting requests and keep it.
UnblockWaitingRequests(session_key,
                       is_persistent_error
                           ? RefreshResult::kFatalError
                           : RefreshResult::kTransientSigningError);
```
