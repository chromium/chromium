---
name: ios/avoid-legacy-ui-bundled-directories
description: In ios/chrome/browser GN files, separate coordinator orchestration and UI presentation into distinct targets, avoid legacy ui_bundled directories, and reject hybrid coordinator_ui targets
rubric: design
severity: should_fix
citations:
  - path: ios/chrome/browser/shared/README.md
    heading: Shared folder
    quote: 'This folder contains the code shared by several features. Please add code to one of subfolder: * `coordinator/` if the code is *only* shared by coordinators/mediators * `model/` if the code is *only* shared by model objects * `ui/` if the code is *only* shared by ui elements * `public/` if the code is shared by all'
---

**Rule.** In `ios/chrome/browser/`, decouple coordinator orchestration from UI
presentation across directory structures and GN build targets:

1. **Directory layout**: Never create a `ui_bundled/` directory for a new
   feature, subfeature, or UI component. Structure feature code into dedicated
   modular subdirectories—such as `coordinator/`, `ui/`, `model/`, and
   `public/`—each with its own `BUILD.gn` target and scoped `DEPS`. Standalone
   test suites or non-feature directories (such as `integration_tests/`) must
   place build targets directly within their root directory rather than nesting
   them under `ui_bundled/`. Existing `ui_bundled/` directories are a
   transitional state: actively working on code in one is fine, provided the
   work includes a plan to factor that code into proper `coordinator/` and `ui/`
   directories.
2. **Build target separation**: In UI build files (including `ui/BUILD.gn`,
   `coordinator/BUILD.gn`, and legacy `ui_bundled/BUILD.gn`), do not bundle view
   classes (views, view controllers, consumers, mutators, cells) into the same
   build target as orchestration classes (coordinators and mediators), and never
   use hybrid target names like `source_set("coordinator_ui")`. Separate
   presentation components into a dedicated `source_set("ui")` target and
   orchestration components into `source_set("coordinator")` (a
   `source_set("ui_bundled")` only exists inside an existing legacy
   `ui_bundled/` directory that has not been factored out yet).

**Why.** Chrome on iOS enforces a strict Coordinator-Mediator-View architecture
where presentation views are decoupled from coordination and scene
orchestration. Bundling view classes and coordinators into monolithic targets or
hybrid `coordinator_ui` targets prevents external callers from depending
strictly on UI presentation elements without compiling or transitively linking
coordinator and scene dependencies. Symmetrically, treating legacy `ui_bundled/`
directories as transitional—never creating new ones and factoring existing ones
into dedicated `coordinator/`, `ui/`, `model/`, and `public/` directories as
they are worked on—enforces clean modular layering and scoped DEPS boundaries,
while keeping standalone test suites at their root avoids directory clutter and
preserves clear ownership.

## Examples

Bad:

```cpp
# Variant 1: Creating a ui_bundled/ directory for a new feature, or nesting
# standalone tests under one
# ios/chrome/browser/autofill/bottom_sheet/ui_bundled/BUILD.gn
source_set("ui_bundled") {
  sources = [
    "payments_bottom_sheet_coordinator.mm",
    "payments_bottom_sheet_view_controller.mm",
  ]
}
# ios/chrome/test/earl_grey2/BUILD.gn
deps = [ "//ios/chrome/browser/integration_tests/ui_bundled:eg2_tests" ]

# Variant 2: Hybrid coordinator_ui target or bundling views and coordinators together
# ios/chrome/browser/feature/BUILD.gn
source_set("coordinator_ui") {
  sources = [
    "feature_coordinator.mm",
    "feature_view_controller.mm",
  ]
}
```

Good:

```cpp
# Variant 1: Modular directories for a new feature, and standalone tests at root
# ios/chrome/browser/autofill/bottom_sheet/coordinator/BUILD.gn
source_set("coordinator") {
  sources = [ "payments_bottom_sheet_coordinator.mm" ]
  deps = [ "//ios/chrome/browser/autofill/bottom_sheet/ui" ]
}
# ios/chrome/browser/autofill/bottom_sheet/ui/BUILD.gn
source_set("ui") {
  sources = [ "payments_bottom_sheet_view_controller.mm" ]
}
# ios/chrome/test/earl_grey2/BUILD.gn
deps = [ "//ios/chrome/browser/integration_tests:eg2_tests" ]

# Variant 2: Decoupled :coordinator and :ui targets
# ios/chrome/browser/feature/coordinator/BUILD.gn
source_set("coordinator") {
  sources = [ "feature_coordinator.mm" ]
  deps = [ "//ios/chrome/browser/feature/ui" ]
}
# ios/chrome/browser/feature/ui/BUILD.gn
source_set("ui") {
  sources = [ "feature_view_controller.mm" ]
}
```
