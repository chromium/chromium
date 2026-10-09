# IWA Key Distribution Provider (`components/webapps/isolated_web_apps/key_distribution`)

This directory implements the runtime provider for Isolated Web App key
rotation, managed and user-install allowlists (with entitlements), blocklists,
and special app permissions.

## Key Components

- `IwaRuntimeDataProvider` (`iwa_runtime_data_provider.{h,cc}`): Abstract
  interface providing access to runtime key rotation, managed allowlist,
  user-install allowlist, blocklist, and special permission data for IWAs.
- `IwaKeyDistributionInfoProvider`
  (`iwa_key_distribution_info_provider.{h,cc}`): Process-wide singleton
  implementing `IwaRuntimeDataProvider` that loads and parses
  `IwaKeyDistribution` protobufs delivered by Component Updater (or preloaded
  defaults) and notifies subscribers via `OnRuntimeDataChanged()`.
- `IwaEntitlement`, `IwaVersionRange`, `IwaEntitlementsSet`, &
  `GetEntitlementForFeature()` (`iwa_entitlements.{h,cc}`): Maps Permissions
  Policy features to required entitlements for user-installed IWAs.
- `iwa_key_distribution_histograms.h`: UMA histogram names and status enums
  (`IwaComponentUpdateError`, `KeyDistributionComponentSource`, etc.) for key
  distribution component loading and queries.
- `proto/`: Protobuf definitions (`key_distribution.proto`) for
  `IwaKeyDistribution`, including `IwaKeyRotations`, `IwaSpecialAppPermissions`,
  and `IwaAccessControl` (managed allowlist, blocklist, and user-install
  allowlist with version-ranged entitlements).
