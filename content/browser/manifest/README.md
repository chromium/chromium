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
In `ManifestManagerHost`, the browser enforces strict same-origin invariants on
`start_url`, `id`, `scope`, and action handlers (`share_target`,
`file_handlers`), terminating any violating renderer process via
`mojo::ReportBadMessage`.

For manifest parsing and Mojo type definitions in Blink, see
[`third_party/blink/renderer/modules/manifest/`](/third_party/blink/renderer/modules/manifest/README.md).
