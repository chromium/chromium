# IWA Component Test Support (`components/webapps/isolated_web_apps/test_support`)

This directory provides hermetic test doubles and bundle builders for testing
`//components/webapps/isolated_web_apps` and downstream embedders.

## Key Utilities

- `TestSignedWebBundleBuilder` & `TestSignedWebBundle`
  (`test_signed_web_bundle_builder.{h,cc}`): Builder for constructing, signing,
  and serializing test `.swbn` bundles in memory.
- `signing_keys.h`: Re-exports default Ed25519 and ECDSA P-256 test key pairs
  and `SignedWebBundleId`s from `web_package::test` into `web_app::test`.
- `signed_web_bundle_utils.{h,cc}`: Helpers (`ReadResponseBody()`,
  `ReadAndFulfillResponseBody()`) for reading response payloads from a
  `SignedWebBundleReader` or Mojo data pipe in tests.
- `TestIwaClient` (`test_iwa_client.{h,cc}`): Test implementation of `IwaClient`
  that provides a configurable `IwaRuntimeDataProvider` pointer (and allows test
  subclasses to override `GetIwaSourceForRequest()` / `RunWhenAppCloses()`).
- `FakeIwaRuntimeDataProvider` (`fake_iwa_runtime_data_provider.{h,cc}`): Fake
  `IwaRuntimeDataProvider` with `ScopedIwaRuntimeDataUpdate` for mutating key
  rotations, allowlists, blocklists, and special permissions in unit tests.
- `ScopedIwaIdentityValidator` (`scoped_iwa_identity_validator.{h,cc}`): Scoped
  helper that installs `IwaIdentityValidator` as the global
  `web_package::IdentityValidator` singleton for the lifetime of a test.
- `key_distribution/test_utils.{h,cc}`: Builders
  (`KeyDistributionComponentBuilder`) and helpers for injecting or loading fake
  key distribution component data into `IwaKeyDistributionInfoProvider`.
