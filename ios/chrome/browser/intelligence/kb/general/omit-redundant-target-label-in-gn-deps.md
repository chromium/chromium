---
name: ios/omit-redundant-target-label-in-gn-deps
description: When declaring GN dependencies, omit redundant directory-matching target names and reference canonical public targets rather than internal generation, grit, or header subtargets
rubric: design
severity: should_fix
citations:
  - path: build/docs/writing_gn_templates.md
    heading: Style Guide
    quote: Chromium GN files follow [GN's Style Guide](https://gn.googlesource.com/gn/+/main/docs/style_guide.md) with a few additions.
  - path: docs/imported/gn/style_guide.md
    heading: Deps
    quote: Prefer to omit the colon and name when possible. See the [GN implicit names](reference.md#implicit-names).
---

**Rule.** In GN build files (`BUILD.gn`, `*.gni`), do not include the colon and
target name suffix when referencing a dependency whose target name matches its
enclosing directory (e.g., use `"//base"` instead of `"//base:base"`,
`"//components/prefs"` instead of `"//components/prefs:prefs"`, or
`"//ui/base/mojom"` instead of `"//ui/base/mojom:mojom"`). Retain explicit
target names only when the target name differs from its directory or for
relative intra-directory targets (`":target"`). Additionally, depend on a
directory's canonical public target rather than internal generation subtargets
(such as `:*_grit`, e.g., use `"//ios/chrome/app/strings"` instead of
`"//ios/chrome/app/strings:ios_strings_grit"` or
`"//ios/chrome/app/strings:ios_branded_strings_grit"`) or internal
cycle-breaking header subtargets (such as `:*_headers`, e.g., use
`"//ios/chrome/browser/authentication/ui_bundled/signin"` instead of
`":signin_headers"` unless breaking an actual circular dependency).

**Why.** In GN, directory-scoped dependency labels without an explicit target
name automatically resolve to the target matching the directory name. Omitting
redundant target name suffixes adheres to the GN Style Guide and keeps build
files concise. Furthermore, depending on internal generation subtargets (such as
`:*_grit`) or internal header subtargets (`:*_headers`) bypasses target
encapsulation, duplicates build graph edges, and breaks canonical resource
packaging. For example, `//ios/chrome/app/strings` is the canonical public
target that bundles and re-exports both branded and unbranded iOS Chrome string
resources, so depending directly on `ios_strings_grit` or
`ios_branded_strings_grit` fragments dependencies.

## Examples

Bad:

```cpp
deps = [
  "//base:base",
  "//components/prefs:prefs",
  "//ios/chrome/app/strings:ios_branded_strings_grit",
  "//ios/chrome/app/strings:ios_strings_grit",
  "//ios/chrome/browser/authentication/ui_bundled/signin:signin_headers",
  "//ui/base/mojom:mojom",
]
```

Good:

```cpp
deps = [
  "//base",
  "//components/prefs",
  "//ios/chrome/app/strings",
  "//ios/chrome/browser/authentication/ui_bundled/signin",
  "//ui/base/mojom",
]
```
