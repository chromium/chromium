---
name: ios/no-named-cpp-namespaces-in-ios-chrome
description: In //ios/chrome/, place embedder-owned declarations and definitions in the global namespace; reopen existing external or shared namespaces only when C++ or Chromium convention requires it
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Namespaces and Symbol Prefixes
    quote: 'Because `//ios/chrome/` is the top-level application layer and is not linked as a library into other products, do **not** prefix Objective-C classes/protocols and do **not** put C++ code in named namespaces—even when implementing an interface from `//components/<foo>/` (write `class FooClientIOS : public foo::FooClient` in the global namespace; see Chromium C++ guidance on named namespaces). Organize code by directory and clear naming instead.'
  - path: styleguide/c++/c++-dos-and-donts.md
    heading: Named namespaces
    quote: Most code should be in a namespace, with the exception of code under `//chrome`, which may be in the global namespace (do not use the `chrome::` namespace).
---

**Rule.** In `//ios/chrome/`, place embedder-owned declarations and
definitions—including classes (even subclasses of namespaced
`//components/<foo>/` interfaces), enums, types, keyed service factories,
functions, feature flags, and feature constants—in the global namespace. Do not
introduce feature/embedder namespaces (`namespace ios`, `namespace chrome`,
`namespace <feature>`) or place `//ios/chrome/` code inside a component's
namespace (`namespace foo`). Reopening an existing external or shared namespace
is expected when C++ or Chromium convention requires the symbol to live there.
Examples include forward-declaring external types
(`namespace web { class WebState; }`), specializing templates (`std::hash`),
implementing established cross-target `//ios` APIs (`ios::provider`,
`app_group`, `chrome_test_util`), and adding constants to cross-platform
constant namespaces (such as `prefs` or `switches`).

**Why.** `//ios/chrome/` is the top-level application layer and is not linked as
a library into other products. Like `//chrome/`, it keeps embedder-owned
declarations and definitions in the global namespace. Named namespaces organize
lower layers, the standard library, shared APIs, and cross-platform constants.
`//ios/chrome/` reopens them only when a symbol must live there. See
`ios/best_practices.md#namespaces-and-symbol-prefixes` and
`styleguide/c++/c++-dos-and-donts.md#named-namespaces`.

## Examples

Bad:

```cpp
// ios/chrome/browser/foo/model/foo_client_ios.h
// Avoid placing embedder-owned declarations in component, embedder, or
// feature namespaces:
namespace foo {
enum class FooActionType {
  kPrimary,
  kSecondary,
};
}  // namespace foo

namespace ios {
class FooClientIOS : public foo::FooClient {};
}  // namespace ios

namespace foo_feature {
BASE_DECLARE_FEATURE(kFooEnableCustomUpload);
}  // namespace foo_feature
```

Good:

```cpp
// ios/chrome/browser/foo/model/foo_client_ios.h
// Keep embedder-owned enums, component interface subclasses, and feature
// flags in the global namespace:
enum class FooActionType {
  kPrimary,
  kSecondary,
};

class FooClientIOS : public foo::FooClient {};

BASE_DECLARE_FEATURE(kFooEnableCustomUpload);
```
