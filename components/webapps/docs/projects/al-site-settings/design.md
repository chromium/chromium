# Design: Refactor TWA Registration to use Package Name keys

This document outlines the design of the fix for site settings issues for TWAs
on Desktop Android, specifically addressing problems with shared UIDs and
multiple apps for the same origin. This design is implemented at HEAD.

## Goal

Refactor the storage keys in `InstalledWebappDataRegister` to use the Android
Package Name instead of the Android UID. This solves issues with multiple apps
claiming the same origin and apps sharing a UID (like Calendar).

## Background

Chrome tracks installed web apps (TWAs and WebAPKs) to manage permissions and
browsing data. It previously used the Android UID assigned at install time as
the key to store this data in `SharedPreferences`.

## Problem Statement

1. **Multiple Apps for Same Origin**: `InstalledWebappRegistrar` skipped
   registration for a second app claiming the same origin because it
   deduplicated by origin only.
2. **Shared UID**: Some apps (e.g., Calendar TWA and SyncAdapter) share a UID.
   `InstalledWebappDataRegister` overwrote data (like package name) when the
   second app registered. On uninstall of one package, it cleared all data for
   the shared UID, breaking the other app's association in Chrome.
3. **Imprecise Site Settings Navigation**:
   `TrustedWebActivitySettingsNavigation` queried data by Android UID. For apps
   sharing a UID, this returned merged origins for all sharing apps, failing to
   isolate them in the Site Settings UI.

See [Identifiers](../../identifiers.md) for details on how these IDs relate.

## Design (Package Name keys)

The primary key in `InstalledWebappDataRegister` was switched from Android UID
to **Android Package Name**.

### Key Changes

- **Storage Keys**: Changed keys from `uid.xxx` to `packageName.xxx`.
- **Package List**: Maintains a set of registered package names instead of UIDs.
- **InstalledWebappBroadcastReceiver**: Updated to use `packageName` extracted
  from intent data to query and clear data.
- **TrustedWebActivitySettingsNavigation**: Must be updated to use `packageName`
  directly to query data for Site Settings, ensuring full isolation for apps
  sharing a UID.

### Migration

To migrate from UID-keyed storage to Package Name-keyed storage, the following
steps are taken (triggered in `prefetchPreferences` on a background thread):

1. Read the set of old UIDs from `trusted_web_activity_uids`.
2. For each UID:
   - Read the associated `packageName`, `appName`, `domain` set, and `origin`
     set.
   - If `packageName` is present:
     - Add `packageName` to the new `trusted_web_activity_packages` set.
     - Write data to new keys: `packageName.appName`, `packageName.domain`,
       `packageName.origin`.
     - (Merge domains and origins if the package name already has entries in the
       new store).
   - Delete the old UID-keyed data (`uid.packageName`, `uid.appName`,
     `uid.domain`, `uid.origin`).
3. After iterating all UIDs, remove the `trusted_web_activity_uids` key to mark
   migration complete.

## Alternatives Considered

### Option A: Keep UID keys, fix uninstall logic

- **Pros**: No data migration needed.
- **Cons**: Overwriting persists. Imprecise data clearing on uninstall.

### Option C: Store multiple packages per UID

- **Pros**: Fixes uninstall bug without full migration.
- **Cons**: Origins are still merged per UID, leading to imprecise clearing.

## Conclusion

Option B (Package Name keys) was chosen as the cleanest architectural solution
as it properly isolates apps even if they share a UID, and the package name is
available in all relevant broadcasts.

## References

- **Bug ID**: 478250911 (Site Settings for TWAs on Desktop Android)
