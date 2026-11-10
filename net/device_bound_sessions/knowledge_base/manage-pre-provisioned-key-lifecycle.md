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
`kPreProvisionedKeyAccessNotGranted`; (2) consume a key only when an SSO
registration's fetch succeeds (`ConsumePreProvisionedKey()` in
`OnRegistrationCompleteInternal()`), failing the registration if the key is gone
by then, and never from `AddSession()` or restore-from-store; (3) delete every
key that is dropped without being consumed, such as on clearing site data or in
the destructor, through the key service; (4) test the access-denied and success
paths. Flag any unusable key that is kept until the relying party's site data is
cleared or the browser shuts down.

**Why.** Pre-provisioned keys are kept until a registration consumes them, so a
key stored without the cookie-access check can't be used but still holds a
hardware key until the relying party's site data is cleared or the browser
restarts (on macOS, until garbage collection deletes the Keychain item after the
next startup), and skipping the check on lookup would let a relying party use a
key after the identity provider lost cookie access. Consuming keys outside
registration completion, such as in `AddSession()` or when restoring sessions
from the store, ties a key's lifetime to unrelated session operations. Removing
a key from the map doesn't delete it from the key service, leaving cleared or
unclaimed keys in the key store until startup or profile garbage collection
runs.

## Examples

Bad:

```cpp
bool SingleSignOnKeyManager::AddPreProvisionedKey(...) {
  // Stores the key without checking that the IdP has cookie access.
  ...  // Insert into `pre_provisioned_keys_`; false for a duplicate.
}
```

Good:

```cpp
bool SingleSignOnKeyManager::AddPreProvisionedKey(...) {
  if (!CanAccessPreProvisionedKey(has_cookie_access_cb_, provider_origin,
                                  rp_origin)) {
    return false;
  }
  ...  // Insert into `pre_provisioned_keys_`; false for a duplicate.
}
```
