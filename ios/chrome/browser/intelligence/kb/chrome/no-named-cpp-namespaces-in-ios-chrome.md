---
name: ios/no-named-cpp-namespaces-in-ios-chrome
description: When declaring C++ classes, enums, functions, or feature flags in //ios/chrome/, keep declarations in the global namespace rather than wrapping in named namespaces
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Namespaces and Symbol Prefixes
    quote: Because `//ios/chrome/` is the top-level application layer and is not linked as a library into other products, do **not** prefix Objective-C classes/protocols and do **not** put C++ code in named namespaces (use anonymous namespaces in `.mm`/`.cc` files for file-local helpers and constants).
---

**Rule.** In `//ios/chrome/`, place embedder declarations—including classes,
enums, types, keyed service factories, functions, constants, and feature
flags—in the global namespace. Do not wrap them in `namespace ios`,
`namespace chrome`, or component/feature namespaces (such as
`namespace autofill`, `namespace password_manager`, or
`namespace <feature>_feature`). Rely on directory hierarchy and class prefixes
or suffixes for scoping (except for provider interfaces under `ios::provider`).

**Why.** Application-level Chrome code under `//ios/chrome/` mirrors `//chrome/`
as an embedder and lives in the global namespace per Chromium style conventions.
Wrapping embedder code or UI enums in `namespace ios` or reusable component
namespaces (`//components/<name>`) blurs architectural boundaries and causes
lookup ambiguities. See `ios/best_practices.md#namespaces-and-symbol-prefixes`.

## Examples

Bad:

```cpp
// Avoid wrapping component/feature enums, classes, or flags in named namespaces:
namespace autofill {
enum class SaveCardActionType {
  kLocal,
  kUpload,
};
}  // namespace autofill

namespace ios {
class OnDeviceTailModelServiceFactory : public ProfileKeyedServiceFactoryIOS {};
}  // namespace ios

namespace password_manager {
BASE_DECLARE_FEATURE(kPasswordManagerEnableCrowdsourcingUploads);
}  // namespace password_manager
```

Good:

```cpp
// Keep embedder types, enums, classes, and flags in the global namespace:
enum class SaveCardActionType {
  kLocal,
  kUpload,
};

class OnDeviceTailModelServiceFactory : public ProfileKeyedServiceFactoryIOS {};

BASE_DECLARE_FEATURE(kPasswordManagerEnableCrowdsourcingUploads);
```
