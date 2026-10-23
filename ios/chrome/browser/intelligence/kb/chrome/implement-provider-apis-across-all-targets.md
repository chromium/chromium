---
name: ios/implement-provider-apis-across-all-targets
description: When adding or modifying provider APIs in ios/public/provider/, implement Chromium and test stubs, and split call sites into follow-up CLs
rubric: design
severity: should_fix
citations:
  - path: ios/public/provider/README.md
    quote: This directory exists to allow Chromium code to call iOS code that is not yet upstreamed. The main entry point from Chromium into this directory is via global Provider objects.
---

**Rule.** When adding or modifying provider API declarations in
`ios/public/provider/**`, implement them across open-source Chromium
(`ios/chrome/browser/providers/**`) and test (`ios/chrome/test/providers/**`)
targets, and keep downstream implementations synchronized. When introducing a
new provider API, split the work across separate CLs: land the public API
declaration and open-source/test stubs in an initial CL, and defer call sites or
feature hooks in `ios/chrome/` to a subsequent CL after downstream
implementations have landed and rolled. When modifying existing provider
declarations, land downstream stubs first before upstream changes to prevent
link errors.

**Why.** Provider APIs define an abstraction boundary allowing Chromium code to
call proprietary iOS code in downstream repositories. Because downstream code
resides in a separate repository and rolls Chromium asynchronously, bundling
call sites or un-stubbed declarations upstream breaks downstream builds with
undefined symbol linker errors during rolls.

## Examples

Bad:

```cpp
// 1. Modifying existing provider API without downstream implementations:
// In ios/public/provider/chrome/browser/foo/foo_api.h:
namespace ios::provider {
void ConfigureFooSuggestions(NSArray<NSString*>* suggestions);
}  // Missing implementations in chromium_foo.mm, test_foo.mm, and downstream.

// 2. Bundling call sites in the same CL when introducing a new provider API:
// In ios/chrome/browser/profile/model/keyed_service_factories.mm:
if (IsFooEnabled()) {
  ios::provider::EnsureFooProfileKeyedServiceFactoriesBuilt();
}
```

Good:

```cpp
// 1. Existing provider changes: stub across targets before upstream declaration:
// In ios/chrome/browser/providers/foo/chromium_foo.mm:
namespace ios::provider {
void ConfigureFooSuggestions(NSArray<NSString*>* suggestions) {}
}  // Also implemented in test_foo.mm and downstream before changing foo_api.h.

// 2. New provider API: split declaration and stubs from call sites across CLs:
// CL 1: Declare in foo_api.h and stub in chromium_foo.mm and test_foo.mm.
// CL 2 (after downstream implements the API and rolls into Chromium):
if (IsFooEnabled()) {
  ios::provider::EnsureFooProfileKeyedServiceFactoriesBuilt();
}
```
