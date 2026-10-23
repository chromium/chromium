---
name: ios/depend-on-granular-feature-targets-in-earlgrey-tests
description: In EarlGrey 2 test targets (eg2_tests), depend on granular feature targets rather than monolithic browser or component implementation libraries when referencing feature flags
rubric: design
severity: should_fix
citations:
  - path: docs/ios/testing.md
    heading: Accessing app internals
    quote: EG2 test targets are built with test-related code but without app code.
---

**Rule.** In EarlGrey 2 test targets (`source_set("eg2_tests")`), depend on
granular feature targets (such as `:features` or `:*_features`) rather than
monolithic browser or component implementation libraries when referencing
feature flags. Do not add full component, model, or browser targets (such as
`//components/.../browser` or `//ios/chrome/browser/...`) to `eg2_tests`
dependencies.

**Why.** EarlGrey 2 test targets run in a separate test runner process built
without application or browser code. Depending on monolithic browser or
component implementation targets pulls transitive application dependencies into
the test runner target where those dependencies are absent, causing build
failures and linker errors.

## Examples

Bad:

```cpp
// Bad: Depending on a monolithic browser component library in eg2_tests.
source_set("eg2_tests") {
  sources = [ "new_tab_page_egtest.mm" ]
  deps = [
    "//base/test:test_support",
    "//components/omnibox/browser",
  ]
}
```

Good:

```cpp
// Good: Depending on the granular feature target for the required feature flags.
source_set("eg2_tests") {
  sources = [ "new_tab_page_egtest.mm" ]
  deps = [
    "//base/test:test_support",
    "//components/omnibox/browser:aim_eligibility_service_features",
  ]
}
```
