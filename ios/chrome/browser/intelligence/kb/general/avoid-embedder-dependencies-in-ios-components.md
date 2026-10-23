---
name: ios/avoid-embedder-dependencies-in-ios-components
description: When configuring DEPS, build targets, or sources in ios/web or ios/components, do not add dependencies on embedders or Chrome provider APIs
rubric: design
severity: should_fix
citations:
  - path: ios/web/README.md
    heading: ios/web vs ios/components vs components/
    quote: 'ios/web: The renderer-equivalent on iOS. It does not depend on higher-level features found in ios/chrome or ios/web_view.'
  - path: ios/components/README.md
    quote: This means that components must not depend on either ios/web_view or ios/chrome.
---

**Rule.** Do not add dependencies on embedders (such as `ios/chrome` or
`ios/web_view`) or embedder-specific provider APIs (such as
`//ios/public/provider/chrome/...`) in `ios/web/` or `ios/components/` source
files, `BUILD.gn` build targets, or `DEPS` include rules. Keep these layers
reusable across all embedders by depending only on lower-level libraries
(`//base`, `//components`, `//ios/net`, `//ios/web/public`, or shared
components); invert dependencies by exposing configuration interfaces,
protocols, or delegates in `ios/web/public/` for embedders to implement and
inject.

**Why.** `ios/web` and `ios/components` are core shared layers designed to
support multiple embedders (including `ios/chrome` and `ios/web_view`).
Depending on an embedder or Chrome-specific provider interfaces breaks
architectural layering, couples core web runtime and shared components to a
specific client application, introduces circular build dependencies, and
prevents reuse by other embedders.

## Examples

Bad:

```cpp
# In ios/web/annotations/BUILD.gn:
source_set("annotations") {
  deps = [
    "//ios/public/provider/chrome/browser/context_menu:context_menu_api",
    "//ios/web/public",
  ]
}

# In ios/components/cookie_util/DEPS:
include_rules = [
  "+ios/chrome/browser/shared/model/application_context",
]
```

Good:

```cpp
# In ios/web/annotations/BUILD.gn:
source_set("annotations") {
  deps = [
    "//ios/web/public",
  ]
}

# In ios/components/cookie_util/DEPS:
include_rules = [
  "+components/prefs",
  "+ios/web/public",
]
```
