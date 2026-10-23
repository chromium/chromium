---
name: ios/scope-deps-include-rules-to-innermost-directory
description: When defining in-tree DEPS include rules, place DEPS in layer subdirectories, allow directory paths over individual headers, and use specific_include_rules for single-file exceptions
rubric: design
severity: should_fix
citations:
  - path: docs/dependencies.md
    heading: Understanding In-Tree `DEPS` Files
    quote: "include_rules: This is the most important variable. It's a list of strings that define which directories are allowed to be included by files within the current directory (and its subdirectories)."
  - path: agents/skills/gn-deps-debugging/references/dependency-patterns.md
    heading: DEPS include rules
    quote: '- Prefer the narrowest `specific_include_rules` entry. - Match existing patterns in the nearest `DEPS` file. - Do not add broad directory-wide allow rules for a single file.'
---

**Rule.** When defining dependencies in in-tree `DEPS` files, place dedicated
`DEPS` files in the innermost subdirectory or architectural layer subfolder
(such as `coordinator/`, `ui/`, `model/`, or `public/`—or an existing legacy
`ui_bundled/`—under `ios/chrome/browser/`) rather than omitting them or placing
broad rules at a parent feature root. In `include_rules`, specify allowed
dependencies by directory path (such as `+components/<subsystem>/public` or
`+ios/chrome/browser/<subsystem>/model`) rather than itemizing individual `.h`
header files. When only a single source file requires a header exception, do not
add a broad directory-wide allow entry to `include_rules`; use
`specific_include_rules` scoped strictly to that specific file and allowed
header (header-level entries belong only in `specific_include_rules`).

**Why.** In-tree `DEPS` rules are inherited by descendant directories during
`checkdeps` enforcement, and the root `DEPS` file blocks cross-feature imports
by default (e.g., `'-ios/chrome/browser'`). Placing `DEPS` at a parent feature
root or broad parent directory leaks dependencies across layers—such as exposing
UI components to model or coordinator internals. Conversely, itemizing
individual header files in `include_rules` creates overly brittle entries that
churn whenever files within a module are added or renamed. Allowing directory
paths in `include_rules` while isolating single-file exceptions to
`specific_include_rules` preserves strict architectural layering without
unnecessary maintenance overhead.

## Examples

Bad:

```cpp
// 1. Placing DEPS at parent feature root instead of layer subfolders:
// ios/chrome/browser/settings/manage_accounts/DEPS
include_rules = [
  "+ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h",
  "+ios/chrome/browser/signin/model/authentication_service.h",
]

// 2. Itemizing individual headers in include_rules instead of directory path:
// ios/chrome/browser/autofill/autofill_ai/coordinator/DEPS
include_rules = [
  "+ios/chrome/browser/signin/model/authentication_service.h",
  "+ios/chrome/browser/signin/model/authentication_service_factory.h",
]

// 3. Broad directory rule in include_rules when only one file needs an exception:
// ios/chrome/browser/settings/autofill/autofill_ai/DEPS
include_rules = [
  "+ios/chrome/browser/autofill/model",
]
```

Good:

```cpp
// 1. Dedicated DEPS in layer subfolder with model directory allowed directly:
// ios/chrome/browser/autofill/autofill_ai/coordinator/DEPS
include_rules = [
  "+ios/chrome/browser/autofill/autofill_ai/public",
  "+ios/chrome/browser/autofill/autofill_ai/ui",
  "+ios/chrome/browser/shared/coordinator/chrome_coordinator",
  "+ios/chrome/browser/signin/model",
]

// 2. UI layer DEPS scoped narrowly (directory path, not a header) to prevent
//    model/coordinator leaks:
// ios/chrome/browser/settings/manage_accounts/ui/DEPS
include_rules = [
  "+ios/chrome/browser/authentication/ui_bundled/cells",
]

// 3. Single-file exception isolated in specific_include_rules:
// ios/chrome/browser/settings/autofill/autofill_ai/ui/DEPS
specific_include_rules = {
  "autofill_ai_add_entities_menu_builder.mm": [
    "+ios/chrome/browser/autofill/model/autofill_ai_util.h",
  ],
}
```
