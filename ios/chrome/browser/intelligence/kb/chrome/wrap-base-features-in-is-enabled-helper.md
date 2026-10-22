---
name: ios/wrap-base-features-in-is-enabled-helper
rubric: readability
severity: should_fix
---

**Rule.** When declaring a new `base::Feature` flag (`BASE_DECLARE_FEATURE`) in
iOS Chrome, define an accompanying `bool Is<Feature>Enabled()` helper function
in the header and implement it in the corresponding `.mm` or `.cc` file within
the same change, rather than deferring the helper or exposing raw feature flags.
Ensure the helper function name strictly matches the `k<Feature>` constant name
(avoiding extraneous words, synonyms, or typos). Callers should always invoke
the companion `Is<Feature>Enabled()` helper instead of querying
`base::FeatureList::IsEnabled` directly. Additionally, when checking an existing
or shared feature flag across multiple call sites within a component or
mediator, extract the flag query into a dedicated helper function or static
class method (e.g., `DataControlsTabHelper::IsSearchWithFeatureEnabled()`)
instead of repeating raw `base::FeatureList::IsEnabled()` queries.

**Why.** Encapsulating `base::FeatureList::IsEnabled()` inside a dedicated
helper function adheres to iOS Chrome conventions, centralizes flag evaluation
logic, and provides a clean, single location to add preconditions (such as iOS
version or device constraints), Finch experiment arm evaluations, or rollout
cleanup without modifying call sites across the codebase. Defining the companion
helper in the same change prevents raw flag usage from leaking into other files.
Strictly matching the helper name to the `k<Feature>` constant prevents naming
divergence, typos, and mismatches across declarations.

## Examples

Bad:

```cpp
// Flag declared without companion helper; callers query raw FeatureList:
BASE_DECLARE_FEATURE(kAddTabGroupColor);
if (base::FeatureList::IsEnabled(kAddTabGroupColor)) { ... }

// Helper name diverges from feature constant (extra 'From'):
BASE_DECLARE_FEATURE(kPasswordRemovalFromDeleteBrowsingData);
bool IsPasswordRemovalFromDeleteFromBrowsingDataEnabled();

// Repeating raw flag check instead of extracting a helper:
if (base::FeatureList::IsEnabled(data_controls::kDataControlsSearchWith)) { ... }
```

Good:

```cpp
// Companion helper declared in header and implemented in .mm in same change:
BASE_DECLARE_FEATURE(kAddTabGroupColor);
bool IsAddTabGroupColorEnabled();
if (IsAddTabGroupColorEnabled()) { ... }

// Helper name strictly matches k<Feature> constant:
BASE_DECLARE_FEATURE(kPasswordRemovalFromDeleteBrowsingData);
bool IsPasswordRemovalFromDeleteBrowsingDataEnabled();

// Extracted dedicated helper for component checks:
static bool IsSearchWithFeatureEnabled() {
  return base::FeatureList::IsEnabled(data_controls::kDataControlsSearchWith);
}
```
