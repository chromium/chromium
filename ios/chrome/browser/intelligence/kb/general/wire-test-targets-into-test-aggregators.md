---
name: ios/wire-test-targets-into-test-aggregators
description: In BUILD.gn, define leaf test targets in their own subfolders and wire directly into root aggregators rather than nesting unit_tests; use test_support for utilities
rubric: design
severity: should_fix
citations:
  - path: docs/ios/testing.md
    heading: Interacting with the app in a test > Accessing app internals
    quote: See `eg_test_support+eg2` (test side utilities) and `eg_app_support+eg2` (app side utilities) targets in `BUILD.gn` files to learn how test utilities are organized in targets.
---

**Rule.** In GN build files (`BUILD.gn`), define unit test targets for test
files in the directory or subfolder where they reside (whenever that subfolder
has its own dedicated `BUILD.gn`), rather than compiling subfolder test sources
in a parent directory's `BUILD.gn`. Test targets must only compile tests for
their own directory. Wire leaf test targets directly into the root test suite
aggregators: `*_unittest.mm` files belong in a `source_set("unit_tests")` listed
in `test("ios_chrome_unittests")` in `ios/chrome/test/BUILD.gn` (or
`ios_web_unittests` in `ios/web/BUILD.gn`), and `*_egtest.mm` files belong in a
`source_set("eg2_tests")` (with `//build/config/ios:xctest_config`) listed in
the `deps` of a suitable `chrome_ios_eg2_test(...)` suite in
`ios/chrome/test/earl_grey2/BUILD.gn`; an `*_egtest.mm` never goes in a
`unit_tests` target. Do not nest child `unit_tests` or `eg2_tests` targets in
`deps` of a parent or sibling component's test target, nor create intermediate
forwarding targets that merely re-export child test targets. When moving or
modularizing test files, update the target in the destination `BUILD.gn` and
remove stale entries from origin files. Additionally, if tests require shared
fixtures, fakes, or utilities from another component, factor those helpers into
a dedicated `:test_support` target marked `testonly = true` and depend on
`:test_support` instead of depending on `:unit_tests`.

**Why.** Chromium test runners aggregate leaf test targets explicitly at the
root suite level rather than discovering files dynamically or traversing nested
subcomponent test targets, and EarlGrey tests are built into separate
`chrome_ios_eg2_test` application modules with the XCTest configuration, so an
`*_egtest.mm` listed in a `unit_tests` target either fails to build or never
runs. Listing subfolder test sources in a parent `BUILD.gn` causes dependency
bloat and breaks modular encapsulation. Conversely, nesting test targets as
dependencies of one another or routing through forwarding targets creates
redundant dependencies, risks duplicate symbol definitions at link time,
obscures test suite ownership, and introduces unnecessary build graph
indirection. Omitting a new test target from the root aggregator silently skips
CI execution. Placing shared fixtures and fakes into dedicated `test_support`
targets preserves clean modular boundaries without coupling components to entire
unit test suites.

## Examples

Bad:

```cpp
// Bad: Listing subfolder test sources in parent BUILD.gn, nesting unit_tests, or using forwarding targets
// In parent/BUILD.gn:
source_set("unit_tests") {
  testonly = true
  sources = [
    "parent_unittest.mm",
    "subfolder/subfolder_unittest.mm",
  ]
  deps = [
    "//base",
    "//extra/subfolder/dep",
  ]
}

// In ios/chrome/browser/settings/personalize_google_services/coordinator/BUILD.gn:
source_set("unit_tests") {
  testonly = true
  sources = [ "personalize_coordinator_unittest.mm" ]
  deps = [
    ":coordinator",
    "//ios/chrome/browser/settings/personalize_google_services/ui:unit_tests",
  ]
}

// In ios/chrome/browser/download/ui/BUILD.gn:
source_set("unit_tests") {
  testonly = true
  deps = [ "//ios/chrome/browser/download/ui/download_list:unit_tests" ]
}

// Bad: EarlGrey test compiled into a unit test target
source_set("unit_tests") {
  testonly = true
  sources = [
    "foo_egtest.mm",
    "foo_unittest.mm",
  ]
}
```

Good:

```cpp
// Good: Subfolder defines its own leaf test target; root aggregator wires leaf targets directly
// In parent/subfolder/BUILD.gn:
source_set("unit_tests") {
  testonly = true
  sources = [ "subfolder_unittest.mm" ]
  deps = [ "//extra/subfolder/dep" ]
}

// In ios/chrome/browser/settings/personalize_google_services/coordinator/BUILD.gn:
source_set("unit_tests") {
  testonly = true
  sources = [ "personalize_coordinator_unittest.mm" ]
  deps = [
    ":coordinator",
    "//ios/chrome/browser/settings/personalize_google_services/ui:test_support",
  ]
}

// In ios/chrome/test/BUILD.gn under test("ios_chrome_unittests"):
test("ios_chrome_unittests") {
  deps = [
    "//ios/chrome/browser/download/ui/download_list:unit_tests",
    "//ios/chrome/browser/settings/personalize_google_services/coordinator:unit_tests",
    "//ios/chrome/browser/settings/personalize_google_services/ui:unit_tests",
    "//parent/subfolder:unit_tests",
  ]
}

// Good: EarlGrey tests in their own eg2_tests target, wired into a suite in
// ios/chrome/test/earl_grey2/BUILD.gn
// In parent/subfolder/BUILD.gn:
source_set("eg2_tests") {
  configs += [ "//build/config/ios:xctest_config" ]
  testonly = true
  sources = [ "foo_egtest.mm" ]
  deps = [
    "//ios/chrome/test/earl_grey:eg_test_support+eg2",
    "//ios/testing/earl_grey:eg_test_support+eg2",
  ]
  frameworks = [ "UIKit.framework" ]
}

// In ios/chrome/test/earl_grey2/BUILD.gn:
_ui_eg2tests_deps = [
  "//parent/subfolder:eg2_tests",
]
chrome_ios_eg2_test("ios_chrome_ui_eg2tests_module") {
  deps = _ui_eg2tests_deps
}
```
