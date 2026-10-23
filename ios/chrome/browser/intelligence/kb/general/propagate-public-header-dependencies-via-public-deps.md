---
name: ios/propagate-public-header-dependencies-via-public-deps
description: In GN targets, declare dependencies (including generated mojom or proto targets) in public_deps only when exposed by public headers, keeping implementation references in deps
rubric: design
severity: should_fix
citations:
  - path: agents/skills/gn-deps-debugging/references/dependency-patterns.md
    heading: deps vs public_deps
    quote: "Use `public_deps` only when consumers of this target's public headers also need the dependency to compile: ... `public_deps` is transitive. Every downstream target inherits it, so adding entries there widens the dependency graph. Treat `public_deps` as part of the API contract."
  - path: agents/skills/gn-deps-debugging/SKILL.md
    heading: Core principles
    quote: 6. **Use `public_deps` only for public API exposure.** If a public header exposes a type from another target in its declarations or inline code, downstream consumers may need that dependency transitively.
---

**Rule.** In GN build files (`BUILD.gn` and `*.gni`), place dependencies in
`public_deps` only when their headers, types, or forwarded configs are exposed
by the target's public or forwarding headers and transitively required by
downstream consumers to compile. Keep dependencies consumed only by
implementation files (`.cc`, `.mm`) or internal headers in private `deps`, and
prune unneeded or unreferenced entries from `public_deps`. Additionally, when a
target's public headers include generated headers (such as from `mojom`,
`proto_library`, or code-generation action targets), declare the generating
dependency in `public_deps` rather than `deps`; keep generated dependencies in
`deps` only when generated headers are exclusively included within
implementation files.

**Why.** Dependencies in `public_deps` propagate transitively to all downstream
targets, widening the build graph, increasing rebuild cascades, and masking
missing direct dependencies from `gn check`. Restricting `public_deps` strictly
to what public headers require enforces module encapsulation. Conversely,
placing public header dependencies in private `deps` breaks downstream
compilation; for generated headers (like mojoms or protobufs), missing
`public_deps` fails to propagate Ninja build ordering transitively, causing
parallel builds to fail flakily with missing-header errors if downstream targets
compile before the generation step finishes.

## Examples

Bad:

```cpp
# Bad 1: Public or generated header dependency placed in deps (breaks downstream compilation/ordering).
source_set("inspect") {
  sources = [ "inspect_ui.h" ]  # Includes "inspect.mojom.h"
  deps = [ ":mojom" ]
}

# Bad 2: Internal implementation dependency or unneeded target exposed in public_deps.
source_set("web_controller") {
  sources = [
    "web_controller.h",
    "web_controller.mm",  # Only .mm uses web_state_impl_header.
  ]
  public_deps = [ "//ios/web/web_state:web_state_impl_header" ]
}
```

Good:

```cpp
# Good 1: Public and generated header dependencies declared in public_deps.
source_set("inspect") {
  sources = [ "inspect_ui.h" ]  # Includes "inspect.mojom.h"
  public_deps = [ ":mojom" ]
}

# Good 2: Implementation-only dependency kept in deps; unused deps pruned.
source_set("web_controller") {
  sources = [
    "web_controller.h",
    "web_controller.mm",
  ]
  deps = [ "//ios/web/web_state:web_state_impl_header" ]
}
```
