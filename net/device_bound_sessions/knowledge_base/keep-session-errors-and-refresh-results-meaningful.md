---
name: net/device_bound_sessions/keep-session-errors-and-refresh-results-meaningful
rubric: correctness
severity: should_fix
---

**Rule.** When a CL adds a `SessionError::ErrorType` or `RefreshResult` value,
or changes which value a failure path reports, check that: (1) the value can
actually occur, and use a `CHECK()` instead if earlier parsing already rules the
condition out; (2) it names one real cause instead of reusing a value for a
different condition; (3) its effect on the session matches its meaning, as
documented in `refresh_result.h` and `session_error.h`, including how
`SessionError::GetDeletionReason()` treats a new value; (4) every mirror is
updated (`device_bound_sessions.mojom`, `enums.xml` and the DevTools mapping)
under a working `LINT.IfChange`/`LINT.ThenChange`; and (5) tests assert the
exact error, not just that no session was created.

**Why.** These values drive session deletion and feed
`Net.DeviceBoundSessions.*` metrics and DevTools. A reused value makes a metric
bucket ambiguous, an unreachable value adds dead handling, a result that
contradicts the session's state misleads callers (`kFatalError` means the
session was terminated), and an error that can change without any test failing
is untested.

## Examples

Bad:

```cpp
// Reuses a value whose name describes a different condition.
if (params.provider_origin.has_value()) {
  return base::unexpected(SessionError::kSessionProviderWellKnownMalformed);
}
```

Good:

```cpp
// A dedicated value (added to the mojom, enums.xml and DevTools mirrors),
// with a test that asserts exactly this error.
if (params.provider_origin.has_value()) {
  return base::unexpected(
      SessionError::kSessionProviderWellKnownHasProviderOrigin);
}
```
