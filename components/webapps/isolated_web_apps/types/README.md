# IWA Core Domain Types (`components/webapps/isolated_web_apps/types`)

This directory defines validated strong value types used across the Isolated Web
Apps stack.

## Key Types

- `IwaOrigin` (`iwa_origin.{h,cc}`): Represents a validated
  `isolated-app://<SignedWebBundleId>` origin created from a `SignedWebBundleId`
  or `GURL`, providing access to `url::Origin`, `SignedWebBundleId`, and storage
  partition domain.
- `IwaVersion` (`iwa_version.{h,cc}`): Validated dotted-integer version (1 to 4
  components) supporting comparison and serialization.
- `IwaSource`, `IwaSourceBundle`, `IwaSourceProxy`,
  `IwaSourceWithModeAndFileOp`, etc. (`source.{h,cc}`): Type-safe hierarchy
  distinguishing production/dev bundles from dev proxy URLs and specifying file
  copy/move semantics.
- `IsolatedWebAppStorageLocation` (`storage_location.{h,cc}`): Persisted storage
  representation (`IwaStorageOwnedBundle`, `IwaStorageUnownedBundle`,
  `IwaStorageProxy`).
- `UpdateChannel` (`update_channel.{h,cc}`): Validated release channel
  identifier (e.g. `"default"`).
- `IsolatedWebAppExternalInstallOptions` & policy constants
  (`isolated_web_app_external_install_options.{h,cc}`,
  `isolated_web_app_policy_constants.{h,cc}`): Parser and configuration types
  for enterprise `IsolatedWebAppInstallForceList` policy entries.
- `IwaUpdateCheckAndPrepareResult` (`update_check_and_prepare_result.{h,cc}`):
  Outcome types (`IwaUpdateCheckAndPrepareSuccess` and
  `IwaUpdateCheckAndPrepareError`) for IWA update discovery and preparation.
- `GeneratedResponse` & `IwaSourceWithModeOrGeneratedResponse`
  (`url_loading_types.h`): Variant types returned by
  `IwaClient::GetIwaSourceForRequest()` when resolving URL requests.
