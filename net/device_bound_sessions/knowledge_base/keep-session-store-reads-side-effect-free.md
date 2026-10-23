---
name: net/device_bound_sessions/keep-session-store-reads-side-effect-free
rubric: correctness
severity: should_fix
---

**Rule.** In `SessionStoreImpl` and its callers, side effects happen only at
defined transitions. Expiry pruning runs only on the first database load
(`OnDatabaseLoaded()`, which passes `prune_expired_sessions` as true); later
reads that reuse `CreateSessionsFromLoadedData()`, such as `GetAllSessions()`,
pass false, record no deletion metrics and must not `CHECK` that nothing
expired. `SaveSession()` writes happen where session state changes, such as
refresh completion, never inside per-request lookups like
`GetSessionsForSite()`. Ask for a test that advances the clock between
`LoadSessions()` and `GetAllSessions()`, and for tests that assert stored
content as well as the absence of writes.

**Why.** Lookups such as `GetSessionsForSite()` run whenever a site with a DBSC
session is accessed, so a database write there turns ordinary browsing into disk
I/O. Pruning again on a later read can delete sessions that expired after load,
crash on a `CHECK` that assumed nothing expired, or record deletion metrics
twice. Keeping side effects at load and at real state changes keeps reads cheap
and repeatable.

## Examples

Bad:

```cpp
// Runs on every request to a site with a session.
std::ranges::subrange<SessionServiceImpl::SessionsMap::iterator>
SessionServiceImpl::GetSessionsForSite(const SchemefulSite& site) {
  ...
  session->RecordAccess();
  // Persists the new access time on the lookup path.
  session_store_->SaveSession(site, *session,
                              SessionStore::SaveSessionMode::kRefresh);
  ...
}
```

Good:

```cpp
// Only the first load prunes expired sessions.
CreateSessionsFromLoadedData(loaded_data, keys_to_delete, sites_to_update,
                             /*prune_expired_sessions=*/true);

// Later reads leave stored data untouched.
CreateSessionsFromLoadedData(loaded_data, keys_to_delete, sites_to_update,
                             /*prune_expired_sessions=*/false);
```
