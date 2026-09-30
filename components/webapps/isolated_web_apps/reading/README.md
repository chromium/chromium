# IWA Signed Web Bundle Reading & Caching (`components/webapps/isolated_web_apps/reading`)

This directory implements reading, signature verification, and reader caching
for Signed Web Bundles (`.swbn`).

## Key Components

- `SignedWebBundleReader` & `UnsecureSignedWebBundleIdReader`
  (`signed_web_bundle_reader.{h,cc}`): Coordinates with
  `data_decoder::SafeWebBundleParser` to read integrity blocks, verify
  signatures via `SignedWebBundleSignatureVerifier`, parse metadata, and read
  individual HTTP responses from a `.swbn` file (or extract an unverified
  `SignedWebBundleId` via `UnsecureSignedWebBundleIdReader`).
- `IsolatedWebAppValidator` (`validator.{h,cc}`): Validates that a bundle's
  integrity block matches the expected `SignedWebBundleId` and trusted keys (via
  `IwaIdentityValidator`) and that bundle metadata contains no primary URL and
  only valid `isolated-app://` exchange URLs.
- `IsolatedWebAppResponseReader` & `IsolatedWebAppResponseReaderImpl`
  (`response_reader.{h,cc}`): Wraps a verified `SignedWebBundleReader` to serve
  responses (stripping query parameters) for a specific IWA.
- `IsolatedWebAppResponseReaderFactory` (`response_reader_factory.{h,cc}`):
  Creates `IsolatedWebAppResponseReader` instances after waiting for best-effort
  runtime data and running `IsolatedWebAppValidator`.
- `IsolatedWebAppReaderRegistry` & `IsolatedWebAppReaderRegistryFactory`
  (`response_reader_registry.{h,cc}`,
  `response_reader_registry_factory.{h,cc}`): Per-`BrowserContext`
  `KeyedService` that caches active `IsolatedWebAppResponseReader` instances
  with idle eviction, key-rotation invalidation, and explicit
  `ClearCacheForPath()` support.
