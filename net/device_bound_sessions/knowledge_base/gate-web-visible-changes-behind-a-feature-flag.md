---
name: net/device_bound_sessions/gate-web-visible-changes-behind-a-feature-flag
rubric: safety
severity: should_fix
---

**Rule.** Put DBSC changes that websites or servers can observe (new or changed
headers, JWT claims, or registration, refresh and deferral behavior) behind a
`features::kDeviceBoundSessions*` flag. Check the flag once, at the entry point
of the new behavior, rather than in every function it reaches. Keep
`BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)` guards consistent between
declarations, definitions and callers, and when a flag is removed, delete every
reference to it and the code path it disabled.

**Why.** A flag lets a web-visible change roll out gradually and be turned off
remotely if it breaks sites, without a revert. A single check at the entry point
keeps the enabled and disabled behavior easy to reason about and to test in both
states. Mismatched build-flag guards break builds where DBSC is compiled out,
and stale flag references leave dead code paths behind.

## Examples

Bad:

```cpp
// New server-visible behavior with no flag: it reaches every user at once and
// can only be turned off with a revert.
if (provider_params->provider_session_id) {
  RegisterFederatedSession(std::move(registration_params));
  return;
}
```

Good:

```cpp
if (provider_params->provider_session_id) {
  // Checked once, where the new behavior starts.
  if (!base::FeatureList::IsEnabled(
          features::kDeviceBoundSessionsFederatedRegistration)) {
    return;  // Ignore federated registration headers while the flag is off.
  }
  RegisterFederatedSession(std::move(registration_params));
  return;
}
```
