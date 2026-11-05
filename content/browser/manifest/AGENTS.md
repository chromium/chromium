# Content Manifest Host (`content/browser/manifest`)

**Parent:** [WebApps Central Hub](/components/webapps/AGENTS.md)

Content layer browser-process coordinator for requesting, fetching, and
downloading manifests and app icons from renderer frames over Mojo. Validates
contextual constraints (such as document origin and manifest URL matching)
against the hosting document context and coordinates embedder overrides via
`MaybeOverrideManifest`.

## Canonical Docs

- [README.md](README.md)
