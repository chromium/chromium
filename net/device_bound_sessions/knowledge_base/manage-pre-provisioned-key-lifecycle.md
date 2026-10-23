---
name: net/device_bound_sessions/manage-pre-provisioned-key-lifecycle
rubric: correctness
severity: should_fix
---

**Rule.** For changes to pre-provisioned SSO keys (created for
`Secure-Session-GenerateKey` and used through `provider_key`): (1) check that
the identity provider has cookie access on the relying party both when
generating or storing a key (`CanGenerateKey()`, `AddPreProvisionedKey()`) and
when looking it up in `FindPreProvisionedKey()`, rejecting denied lookups with
`kPreProvisionedKeyAccessNotGranted`; (2) consume a key only when registration
succeeds (`ConsumeKeyForSession()` in `OnRegistrationCompleteInternal()`), and
never from `AddSession()` or restore-from-store; (3) test the access-denied and
success paths. Flag any unusable key that stays in memory until site data is
cleared or the browser shuts down.

**Why.** Pre-provisioned keys stay in memory and count against a
per-identity-provider limit, so a key stored without the cookie-access check
can't be used but still takes a slot until site data is cleared or the browser
restarts, and skipping the check on lookup would let a relying party use a key
after the identity provider lost cookie access. Consuming keys outside
registration completion, such as in `AddSession()` or when restoring sessions
from the store, ties a key's lifetime to unrelated session operations.

## Examples

Bad:

```cpp
// Stores the key without checking that the IdP has cookie access.
pre_provisioned_keys_.push_back(
    {provider_origin, SchemefulSite(provider_origin), rp_origin,
     std::string(provider_key), key_id});
return true;
```

Good:

```cpp
if (!CanAccessPreProvisionedKey(has_cookie_access_cb_, provider_origin,
                                rp_origin)) {
  return false;
}
pre_provisioned_keys_.push_back(
    {provider_origin, SchemefulSite(provider_origin), rp_origin,
     std::string(provider_key), key_id});
return true;
```
