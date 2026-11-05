# Blink Manifest Common (`third_party/blink/public/common/manifest`)

**Parent:** [WebApps Central Hub](/components/webapps/AGENTS.md)

Validating `blink::Manifest` C++ type, Mojo struct traits
(`manifest_mojom_traits.h`), intrinsic invariant checks
(`manifest_invariants.h`), and cross-platform manifest utilities. All
`blink.mojom.Manifest` IPCs deserialize directly into `blink::Manifest`,
enforcing URL validity, non-opaque origin, same-origin/same-site bounds, and
handler invariants at the Mojo boundary.
