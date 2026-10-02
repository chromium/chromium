# Tracked preferences

## Overview and taxonomy

Tracked preferences are preferences that have their on-disk value tracked with
integrity metadata to prevent the values from being illegitimately changed
out-of-band.

There are three integrity tiers for preferences in Chromium, two of which are
tracked:

```
            Preferences
                 |
       +-------------------+
       |                   |
Untracked prefs      Tracked prefs
                           |
                +---------------------+
                |                     |
        Unprotected prefs      Protected prefs
```

- **Untracked preferences**: Standard preferences. No integrity metadata is
  stored, and no tampering validation occurs.
- **Unprotected preferences**: Tracked for tampering via telemetry. Tampering is
  reported in histograms, but tampered pref values are not changed.
- **Protected preferences** (also called "secure preferences"): Tracked and
  enforced. Tampered values are reset to registered defaults.

### Enforcement and storage mechanisms

To implement these tiers, the system uses three mechanisms:

- **Individual authenticators**: Integrity metadata calculated per preference to
  authenticate individual preference values.
- **Super authenticators**: Integrity metadata calculated over a set of
  preferences to protect against addition or removal of preferences or
  authenticators.
- **Segregated storage**: Protected preferences are stored in a separate file on
  disk from other preferences.

| **Integrity tier** | **Individual authenticator** | **Super authenticator** | **Action on mismatch** | **Storage file** |
| :--- | :--- | :--- | :--- | :--- |
| **Untracked** | ❌ No | ❌ No | None | `Preferences` |
| **Unprotected** | ✅ Yes | ❌ No | Report in histograms | `Preferences` |
| **Protected** | ✅ Yes | ✅ Yes | Reset to default value | `Secure Preferences` |

## Authenticators

To validate the integrity of tracked preference values on disk, this directory
uses integrity metadata called authenticators. When a tracked preference has its
value changed, an authenticator is calculated for the new pref value, then
written to the prefs file to be read later, at validation time.

### Super authenticators

Protected preferences also use a super authenticator: a single authenticator
computed over a set of tracked pref authenticators. Just like individual pref
authenticators, the super authenticator is updated when tracked pref values
change and is read at validation time. Super authenticators are not used for
unprotected prefs.

While authenticators are used to authenticate individual pref values on disk,
the super authenticator is used to validate the *set* of preferences that have
authenticators. If an attacker overwrites a pref value on disk and deletes the
pref's authenticators, then the super authenticator will be invalid, and the
pref's value will be treated as untrusted. In the other direction, if an
existing pref goes from untracked to protected, it won't have any existing
authenticators, so the super authenticator will still be valid, and the
authenticator-less pref will be treated as a legitimate new addition.

### Types of authenticators

Originally, tracked preferences were only validated with an HMAC (hash-based
message authentication code), which anyone can generate, including attackers
seeking to overwrite pref values. To provide better protection against pref
hijacking, tracked prefs are validated with a hash encrypted with OS-provided
encryption when available, via `//components/os_crypt`. For now, HMACs are still
used as a fallback, but are now a legacy authenticator.

Just like authenticators for individual tracked prefs, this directory also uses
two types of super authenticator: super HMACs and super encrypted hashes, and
super encrypted hashes are the modern successor to super HMACs.

When code in this directory is specific to one type of authenticator, it should
be marked as such (e.g., by including `Hmac` or `EncryptedHash` in method
names). Similarly, when code is generic to all types of authenticator, it should
use the umbrella term `Authenticator` (or `AuthData` when dealing with data
containing authenticators).

## Pref tracking strategies

There are two pref tracking strategies: atomic and split. Tracked prefs have
their tracking strategy defined in
[`TrackedPreferenceMetadata::strategy`](../public/mojom/preferences.mojom).

Atomic prefs have their entire value stored at a single pref path. Tracked
atomic prefs are authenticated with a single authenticator for their entire
value, regardless of whether that value is a literal or a dictionary.

Split prefs are dictionaries for which each top-level entry is tracked
independently. Tracked split prefs are authenticated with an authenticator per
top-level entry, so if a tracked split pref has 50 entries, code in this
directory will calculate 50 authenticators for that pref.

## Enforcement levels

Tracked prefs have their enforcement level defined in
[`TrackedPreferenceMetadata::enforcement_level`](../public/mojom/preferences.mojom).
A tracked pref's `enforcement_level` determines whether it's a protected pref or
an unprotected pref.

Unprotected preferences have the enforcement level `NO_ENFORCEMENT`, while
protected preferences have the enforcement level `ENFORCE_ON_LOAD`.

## Resetting pref values

If a protected pref's value fails authentication at load time, the pref's value
and authenticators are removed from the `Secure Preferences` file. Without a
value specified in this file, the
[`PrefService`](../../../components/prefs/pref_service.h) will fall back to its
registered default value for that pref.

When tampering is detected for a protected split pref, resetting can occur two
ways:

- If a top-level key is illegitimately added to a split pref dictionary, the
  whole dictionary is deleted from `Secure Preferences`.
- If a top-level key has its value changed such that it mismatches with its
  authenticator, only that top-level key is deleted from `Secure Preferences`.

## Pref segregation

On disk, untracked prefs and unprotected prefs are stored together in a single
JSON file, while protected prefs are stored in a separate, dedicated JSON file.
[`TrackedPersistentPrefStoreFactory`](./tracked_persistent_pref_store_factory.h)
creates a separate `PrefHashFilter` instance for each of the two files, with the
filter for the protected prefs file having super authenticators enabled.

## Migration

When a preference changes integrity tier, the preference must be migrated to
avoid false-positive resets. Note that while it's possible to migrate a
preference to a lower integrity tier, in practice, preference tracking config
changes usually migrate preferences to a higher integrity tier or deprecate
them entirely.

To migrate a preference from **untracked** to…

- **…unprotected:** No immediate changes are made. (There's no existing
  authenticator for the pref, but also no super authenticator, so the pref value
  is loaded as normal and an authenticator is calculated when the load is
  finalized.)
- **…protected:** The pref value is copied from the unprotected store into the
  protected store in memory. Because the existing protected preferences have a
  valid super authenticator, the new preference is trusted and not reset. An
  authenticator is calculated for it, the super authenticator is updated to
  cover the new set, and the duplicate value in the unprotected store is deleted
  once the protected store is written to disk.

To migrate a preference from **unprotected** to…

- **…untracked:** No immediate changes are made. The pref's authenticators are
  left in `Preferences`, but they are no longer validated or updated.
- **…protected:** The pref value and its existing authenticators are copied from
  the unprotected store into the protected store in memory. The super
  authenticator is updated to include the preference in the protected set, and
  the old value in the unprotected store is deleted once the protected store is
  written to disk.

To migrate a preference from **protected** to…

- **…untracked**: Directly migrating from protected to untracked is unsupported,
  unless the preference is being deprecated.
- **…unprotected**: The pref's value and authenticators are copied from the
  protected prefs store to the unprotected store. The value and authenticators
  are then removed from the protected store and the super authenticator is
  recalculated.

## Core classes

- [`TrackedPersistentPrefStoreFactory`](./tracked_persistent_pref_store_factory.h):
  Sets up the pieces. Creates the `SegregatedPrefStore`, which has one
  `JsonPrefStore` for each of the `Preferences` and `Secure Preferences` files;
  creates the `PrefHashFilters` and migration interceptors.
- [`PrefHashFilter`](./pref_hash_filter.h): Hooks into pref store load and
  serialization events, and receives pref value change notifications.
  Orchestrates validation.
- [`TrackedPreference`](./tracked_preference.h) and subclasses:
  Strategy-specific validation, reset execution, and histogram reporting.
- [`PrefHashStore`](./pref_hash_store.h) /
  [`PrefHashStoreImpl`](./pref_hash_store_impl.h): Holds context for
  authenticator calculation, and provides transactions for reading and writing
  authenticators and super authenticators within that context.
- [`PrefHashCalculator`](./pref_hash_calculator.h): Computes authenticators and
  super authenticators.
- [`HashStoreContents`](./hash_store_contents.h) and subclasses: Adapter for
  authenticator storage backends.

## Execution flows

### Tracked pref value change

1. When a tracked pref has its value changed in memory, its value is queued for
serialization to disk and `PrefHashFilter` is notified of the value change via
`FilterUpdate()`.
2. Some time later, serialization to disk is triggered (by a timer, browser
shutdown, etc), and `PrefHashFilter::FilterSerializeData()` is called.
3. Within a new `PrefHashStoreTransaction`, `PrefHashFilter` calls
`TrackedPreference::OnNewValue()` for every preference that has a new value
since the last serialization to disk.
4. `TrackedPreference` implementations call `PrefHashStoreTransaction` to
compute and store authenticators for the new pref value.
5. `PrefHashStoreTransaction` calls `PrefHashStore` to compute the new
authenticators, then calls `HashStoreContents` to store them.
6. Once all the changed tracked prefs have had new authenticators calculated,
the `PrefHashStoreTransaction` updates the super authenticators as it is being
destroyed.

### Tracked pref integrity validation

1. At browser startup, preferences values are loaded from disk, and
`PrefHashFilter::FinalizeFilterOnLoad()` is called.
2. Within a new synchronous `PrefHashStoreTransaction` (created without an
encryptor), `PrefHashStore` validates the super HMAC against the stored
authenticators.
3. `PrefHashFilter` calls `TrackedPreference::EnforceAndReport()` for each
tracked preference to validate its loaded value against its stored authenticator
in `PrefHashStoreTransaction`.
4. If validation fails for an enforce-on-load tracked pref, `TrackedPreference`
resets or clears the invalid value (or invalid dictionary entries for split
prefs) in the loaded preferences dictionary and updates the stored authenticator
in the transaction.
5. The synchronous `PrefHashStoreTransaction` updates the super HMAC as it is
destroyed, and `PrefHashFilter` passes the filtered preferences to its callback
so browser startup can proceed without waiting for OS encryption keys.
6. If encrypted pref hashing is enabled, `PrefHashFilter` posts a deferred task
to re-validate preferences once the `os_crypt_async::Encryptor` becomes
available.
7. In the deferred pass, a new `PrefHashStoreTransaction` is created with the
`Encryptor` to validate the super encrypted hash.
8. `PrefHashFilter` calls `TrackedPreference::EnforceAndReport()` with the
`Encryptor` for each tracked preference not already reset during the synchronous
pass.
9. If an enforce-on-load preference fails validation against its encrypted hash,
its value is reset to default in the live `PrefService`, and updated
authenticators are scheduled to be flushed to disk.
