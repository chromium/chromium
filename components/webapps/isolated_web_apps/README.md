# Isolated Web Apps Component (`components/webapps/isolated_web_apps`)

This directory contains the core, browser-agnostic logic for Isolated Web Apps
(IWAs). It is designed to be independent of high-level browser features like UI
or profile app management systems (`//chrome`). The focus lies on the
fundamental building blocks of the IWA platform.

## Companion Documentation

- **AI Agent Rules & Invariants:** [AGENTS.md](AGENTS.md)
- **Desktop Browser Engine:**
  [`//chrome/browser/web_applications/isolated_web_apps`](/chrome/browser/web_applications/isolated_web_apps/README.md)
- **Signed Web Bundles Foundation:**
  [`//components/web_package/signed_web_bundles`](/components/web_package/signed_web_bundles/README.md)

## Key Responsibilities

- **Signed Web Bundles:** Logic for parsing `.swbn` files, verifying their
  integrity blocks (signatures and public keys), and extracting metadata. This
  ensures the content of an IWA is authentic and has not been tampered with.
- **Core Types:** The general IWA data types, such as:
  - `IwaOrigin`: The unique origin of an IWA, derived from its Signed Web Bundle
    ID.
  - `IwaVersion`: Handles parsing and comparison of version strings.
  - `IwaSource`: Where an IWA resource originates from (i.e., a local bundle
    file or a proxy for development).
- **Key Distribution & Rotation:** `IwaKeyDistributionInfoProvider` manages
  trusted key rotations, blocklists, and special app permissions delivered via
  Component Updater.
- **URL Loading:** The infrastructure to handle the `isolated-app://` scheme. It
  translates requests into reads from the underlying Signed Web Bundle.
- **Reader Caching:** The `IsolatedWebAppReaderRegistry` maintains a cache of
  bundle readers. This is an optimization to allow repeated requests to reuse
  readers, while managing system resources like file handles and memory.
- **Embedder Bridge:** The `IwaClient` is the interface between this component
  and the rest of the system (e.g. Chrome's web app code).

## Directory Structure

- `client.{h,cc}` & `scheme.h`: Top-level embedder bridge interface
  (`IwaClient`) and `isolated-app` scheme constants (`kIsolatedAppScheme`).
- [`bundle_operations/`](bundle_operations/README.md): Orchestrates high-level
  bundle tasks like ID extraction, signature validation, and reader cache
  eviction.
- [`download/`](download/README.md): Logic for downloading Signed Web Bundles to
  scoped temporary files.
- [`error/`](error/README.md): Defines IWA-specific error types
  (`UnusableSwbnFileError`) and utilities to log `base::expected` results to
  UMA.
- [`identity/`](identity/README.md): Validation of IWA identity and public keys,
  including support for key rotation via `IwaIdentityValidator`.
- [`key_distribution/`](key_distribution/README.md): Component-loaded key
  rotation, allowlist, blocklist, entitlements, and special permissions provider
  (`IwaRuntimeDataProvider`, `IwaKeyDistributionInfoProvider`,
  `IwaEntitlementsSet`) and protobuf definitions.
- [`reading/`](reading/README.md): Low-level logic for parsing and validating
  Signed Web Bundle contents (`SignedWebBundleReader`,
  `IsolatedWebAppValidator`) and caching `IsolatedWebAppResponseReader`
  instances in `IsolatedWebAppReaderRegistry`.
- [`service/`](service/README.md): Base `BrowserContextKeyedServiceFactory`
  (`IsolatedWebAppBrowserContextServiceFactory`) that restricts IWA keyed
  services to eligible browser contexts.
- [`test_support/`](test_support/README.md): Test utilities, including
  `TestSignedWebBundleBuilder`, `FakeIwaRuntimeDataProvider`, and
  `TestIwaClient`.
- [`types/`](types/README.md): Core strong domain types (`IwaOrigin`,
  `IwaVersion`, `IwaSource`, `IsolatedWebAppStorageLocation`, `UpdateChannel`).
- [`url_loading/`](url_loading/README.md): Implementation of the
  `isolated-app://` `URLLoader`, `URLLoaderFactory`, and COOP/COEP/CORP/CSP
  `header_utils` to translate network requests into bundle reads or dev-mode
  proxy forwards.
