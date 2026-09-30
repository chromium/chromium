# Isolated Web Apps Core Component (`components/webapps/isolated_web_apps`)

**Parent:** [WebApps Guidelines](/components/webapps/AGENTS.md)

Browser-agnostic building blocks for Isolated Web Apps (IWAs): Signed Web Bundle
(`.swbn`) reading and caching, `isolated-app://` URL loading, cryptographic
identity verification, IWA key distribution, and core domain types.

## Canonical Docs

- [Component Overview & Subdirectory Map](README.md)
- [Desktop Browser IWA Engine](/chrome/browser/web_applications/isolated_web_apps/AGENTS.md)
- [Signed Web Bundles Foundation](/components/web_package/signed_web_bundles/AGENTS.md)
- [WebApps Developer Skill](/components/webapps/_agents/skills/webapps-dev/SKILL.md)

## Layering & Architectural Invariants

- **Strict Component Layering (`DEPS`):** This component sits below `//chrome`
  and must never depend on `//chrome/*` or internal `//content/browser/*`
  headers (only `//content/public/browser` and narrow dependencies listed in
  `DEPS`). All embedder-specific queries (such as locating installed bundles or
  runtime data) go through `IwaClient` (`client.h`) and `IwaRuntimeDataProvider`
  (`public/iwa_runtime_data_provider.h`).
- **Subpackage Visibility Encapsulation:** Internal subpackage targets (all
  subdirectories except `public/` and `test_support/`) restrict `visibility` to
  `//components/webapps/isolated_web_apps/*`. External consumers must depend on
  `//components/webapps/isolated_web_apps` or
  `//components/webapps/isolated_web_apps/public`.
- **Rule of 2 & Out-of-Process Parsing:** Never parse untrusted CBOR or Web
  Bundle bytes directly on the browser UI/IO thread. Integrity blocks and
  metadata are parsed via `data_decoder::SafeWebBundleParser` and the Rust
  parsers in `//components/web_package`.
- **Strong Domain Types & `base::expected`:** Always represent IWA identifiers
  and versions with validated strong types (`IwaOrigin`,
  `web_package::SignedWebBundleId`, `IwaVersion`, `IwaSource`,
  `IsolatedWebAppStorageLocation`) rather than raw `GURL` or `std::string`.
  Propagate recoverable failures with `base::expected<T, E>`,
  `ASSIGN_OR_RETURN`, and `RETURN_IF_ERROR`.
- **Reader Registry Lifecycle:** `IsolatedWebAppReaderRegistry` caches open
  `IsolatedWebAppResponseReader` (and underlying `SignedWebBundleReader`)
  instances keyed by bundle `base::FilePath`. Callers must use
  `ClearCacheForPath()` before overwriting or deleting `.swbn` files on disk.

## Testing Guardrails

- Prefer fast, deterministic unit tests in `components_unittests` using
  `TestSignedWebBundleBuilder`, `TestIwaClient`, and `base::test::TestFuture`.
- Never use `base::PlatformThread::Sleep()` in tests; use `TaskEnvironment`
  `FastForwardBy()` or `TestFuture` synchronization.
- When asserting UMA histograms with `base::HistogramTester`, wrap
  `histogram_tester.GetAllSamples(...)` inside
  `EXPECT_THAT(..., BucketsAre(...))` or
  `EXPECT_THAT(..., ElementsAre(base::Bucket(...)))`.

## Command Line Execution

- **Component Unit Tests:**
  `tools/autotest.py -C out/Default components/webapps/isolated_web_apps/`
- **Header & Visibility Check:**
  `gn check out/Default "//components/webapps/isolated_web_apps/*"`
