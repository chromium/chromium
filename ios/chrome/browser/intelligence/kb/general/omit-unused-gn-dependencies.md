---
name: ios/omit-unused-gn-dependencies
description: When declaring GN targets or removing unneeded imports, omit unreferenced entries from deps, public_deps, frameworks, ts_deps, and associated DEPS include rules
rubric: readability
severity: should_fix
citations:
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 09. **Delete unused imports and build deps**
    quote: Remove `#include`s, `#import`s, and forward declarations that are no longer needed. - Remove `BUILD.gn` deps that are no longer needed.
  - path: docs/webui/webui_build_configuration.md
    heading: Arguments
    quote: 'ts_deps: List of ts_library() dependencies to generate path mappings for.'
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: '09. **Delete unused imports and build deps**:'
    quote: Remove `#include`s, `#import`s, and forward declarations that are no longer needed. - Remove `BUILD.gn` deps that are no longer needed.
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 09. Delete unused imports and build deps
    quote: Remove BUILD.gn deps that are no longer needed.
---

**Rule.** In GN build files (`BUILD.gn` and `*.gni`), do not include or retain
target dependencies in `deps`, `public_deps`, `frameworks` (or
`weak_frameworks`), or template dependency lists (such as `ts_deps` in
`build_webui`) that are not directly used, linked, or referenced by the target's
source files or public interface. Only list targets whose headers, symbols,
build artifacts, generated files, or resources are actually consumed. For
example, do not add `//testing/gmock` when a target only uses GoogleTest
assertions (`//testing/gtest`), do not list unused system frameworks (such as
`SwiftUI.framework`) in `frameworks`, and do not list library targets in
`ts_deps` when their modules are not imported. Additionally, do not retain
unused `#include` or `#import` directives in source files. When removing or
omitting unneeded headers, deleting source files, or refactoring targets, audit
and prune any corresponding dependencies in `BUILD.gn` (`deps` or `public_deps`)
if no other sources in the target require them. (Pruning the matching `DEPS`
allow rules is covered by `ios/clean-up-obsolete-deps-include-rules`.)

**Why.** Target dependency and framework lists define the build graph and linker
inputs. Declaring unreferenced dependencies or frameworks in `deps`,
`public_deps`, `frameworks`, or `ts_deps` adds unnecessary graph edges, bloats
build caches, and triggers redundant recompilation or relinking when unneeded
upstream targets change. While tools like `gn check` verify that declared
headers satisfy dependency constraints, they do not detect unneeded
dependencies. Furthermore, retaining unused `#include` or `#import` directives
clutters code and lengthens compile times, and failing to prune corresponding
`BUILD.gn` dependencies introduces artificial coupling between targets.

## Examples

Bad:

```cpp
// 1. Unused dependencies and frameworks in BUILD.gn:
source_set("feature_ui") {
  sources = [ "feature_view_controller.mm" ]
  deps = [
    ":feature",
    "//testing/gmock",  // Unused: sources only use GoogleTest.
  ]
  frameworks = [ "SwiftUI.framework" ]  // Unused: no SwiftUI APIs referenced.
}

// 2. Unused import retained alongside a stale BUILD.gn dep:
// In feature_view_controller.mm:
#import "ios/chrome/browser/cobrowse/model/cobrowse_agent.h"  // Unused

// In BUILD.gn deps:
//   "//ios/chrome/browser/cobrowse/model",  // Unused dependency
```

Good:

```cpp
// 1. Only reference consumed dependencies and frameworks in BUILD.gn:
source_set("feature_ui") {
  sources = [ "feature_view_controller.mm" ]
  deps = [ ":feature" ]
}

// 2. Prune unused imports and the corresponding build dependency:
// In feature_view_controller.mm:
// (Only import headers actively referenced by implementation code)

// In BUILD.gn deps:
// Pruned "//ios/chrome/browser/cobrowse/model" since no sources require it.
```
