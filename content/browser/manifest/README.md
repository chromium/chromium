# Content Manifest Subsystem (`content/browser/manifest`)

This directory contains the browser-process coordinator for Web App Manifest
fetching and icon downloading in the Content layer.

## Key Classes

- **[`ManifestManagerHost`](manifest_manager_host.h)**: A
  `content::PageUserData` helper attached to the main frame of a `WebContents`.
  Communicates over Mojo IPC with Blink's renderer-side
  [`ManifestManager`](/third_party/blink/renderer/modules/manifest/manifest_manager.h)
  (`blink::mojom::ManifestManager`) to request and observe web app manifests.
- **[`ManifestIconDownloader`](/content/public/browser/manifest_icon_downloader.h)**:
  Utility exported in Content's public API for downloading and decoding icon
  bitmaps for a web app manifest from the network.

## Security & Trust Boundaries

The browser process does not trust manifest parsing from the untrusted renderer.
Intrinsic invariants on URLs, non-opaque origins, identifiers, scopes, file
handlers, protocol handlers, migrations, shortcuts, scope extensions, and icons
are validated at the Mojo boundary via
`StructTraits<blink::mojom::ManifestDataView, ::blink::Manifest>::Read()`
(defined in
[`manifest_invariants.h`](/third_party/blink/public/common/manifest/manifest_invariants.h)).
`ManifestManagerHost` validates contextual constraints against the hosting
document's execution context (such as non-null manifest on `kSuccess`, document
origin matching, and requested `manifest_url` matching) before delivering the
manifest to embedders or applying overrides via `MaybeOverrideManifest`. Any
violating renderer is terminated via `mojo::ReportBadMessage`.

For manifest parsing and Mojo type definitions in Blink, see
[`third_party/blink/renderer/modules/manifest/`](/third_party/blink/renderer/modules/manifest/README.md).
