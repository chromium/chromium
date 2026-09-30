# IWA Bundle Operations (`components/webapps/isolated_web_apps/bundle_operations`)

This directory provides high-level operations on Signed Web Bundle (`.swbn`)
files used by embedders during installation, updates, and verification.

## Key Operations (`bundle_operations.{h,cc}`)

- `ReadSignedWebBundleIdInsecurely()`: Reads the integrity block of an untrusted
  `.swbn` file via `UnsecureSignedWebBundleIdReader` to extract its
  `SignedWebBundleId` without verifying signatures.
- `ValidateSignedWebBundleSignatures()`: Waits for best-effort runtime data from
  `IwaRuntimeDataProvider`, verifies cryptographic signatures via
  `SignedWebBundleReader`, and validates the integrity block and metadata via
  `IsolatedWebAppValidator`.
- `CloseBundle()`: Closes and evicts cached readers for a bundle path in
  `IsolatedWebAppReaderRegistry` before a bundle file is updated or deleted.
