---
name: net/device_bound_sessions/revalidate-sessions-after-reentrant-calls
rubric: safety
severity: should_fix
---

**Rule.** In `SessionServiceImpl`, treat any call that can run a callback as a
point where sessions may be deleted or replaced. This includes
`UnblockWaitingRequests()`, `NotifySessionAccess()`,
`MaybeStartProactiveRefresh()`, `CompleteProactiveRefresh()`, the
`DeleteSession*()` helpers, observer notifications and completion callbacks,
because they can run code that re-enters the service before returning. After
such a call, don't use a `Session*`, `Session&` or iterator into
`unpartitioned_sessions_` that was obtained before it: copy what you need first
(such as the session ID), or look the session up again by `SessionKey` and
handle it being gone. Don't make such calls while iterating
`unpartitioned_sessions_`, `deferred_requests_` or `proactive_requests_`;
collect the keys or `extract()` the entries first, then act. Across asynchronous
steps (key restoration, signing, fetches, cookie reads), bind the `SessionKey`
by value and look the session up again when the step completes.

**Why.** Deleting a session frees it, so a pointer, reference or iterator kept
across one of these calls can dangle, and erasing from a map while iterating it
skips or revisits entries. These callbacks reach deferred requests, observers
and other parts of the network stack, which can restart a request or delete a
session before returning, so the hazard exists even when the code between the
lookup and the use looks local.

## Examples

Bad:

```cpp
for (auto it = unpartitioned_sessions_.begin();
     it != unpartitioned_sessions_.end(); ++it) {
  if (SessionMatchesFilter(it->first.site, *it->second, created_after_time,
                           created_before_time, origin_and_site_matcher)) {
    // Runs callbacks that can re-enter and erase entries, invalidating `it`.
    DeleteSessionAndNotifyInternal(reason, it, base::NullCallback());
  }
}
```

Good:

```cpp
std::vector<SessionsMap::node_type> sessions_to_delete;
for (auto it = unpartitioned_sessions_.begin();
     it != unpartitioned_sessions_.end();) {
  auto curit = it++;
  if (SessionMatchesFilter(curit->first.site, *curit->second,
                           created_after_time, created_before_time,
                           origin_and_site_matcher)) {
    sessions_to_delete.push_back(unpartitioned_sessions_.extract(curit));
  }
}
// No iterator into the map is held while callbacks run.
for (SessionsMap::node_type& node : sessions_to_delete) {
  DeleteSessionAndNotifyInternal(reason, std::move(node), base::NullCallback());
}
```
