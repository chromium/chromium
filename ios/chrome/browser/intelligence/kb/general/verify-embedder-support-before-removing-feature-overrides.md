---
name: ios/verify-embedder-support-before-removing-feature-overrides
description: When removing explicit feature flag disablements or runtime overrides in embedders, verify why it was disabled and implement required platform dependencies first
rubric: correctness
severity: should_fix
citations:
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 01. **Determine flag's default state**
    quote: If the flag has a uniform default state but is explicitly overridden (e.g., force-disabled) for specific platforms via runtime overrides, bring this discrepancy to the user's attention before cleaning it up.
---

**Rule.** Do not remove explicit feature flag disablements or runtime overrides
in embedders (such as `//ios/web_view`) during feature flag cleanups without
verifying why the feature was disabled and ensuring the target implements all
required platform dependencies. Embedders frequently force-disable features
enabled by default when they lack underlying platform implementations (such as
platform-specific fetchers, delegates, or keyed services); implement and verify
the required platform dependencies first before removing the disablement
override.

**Why.** Embedders and specialized targets often explicitly disable features
that are otherwise enabled by default because the target lacks platform-specific
services or delegates. Deleting the disablement override during flag cleanup
automatically enables the unvetted feature for that target, leading to runtime
failures or crashes due to missing dependencies.

## Examples

Bad:

```cpp
// Removing explicit disablement during flag cleanup before target implements platform support:
std::vector<const base::Feature*> disabled_features;
feature_list->InitFromCommandLine(
    /*enable_features=*/MakeFeaturesString(enabled_features),
    /*disable_features=*/"");
```

Good:

```cpp
// Retain the disablement override until prerequisite platform support is implemented:
std::vector<const base::Feature*> disabled_features;
disabled_features.push_back(&strike_database::features::kDisableStrikeSystem);
feature_list->InitFromCommandLine(
    /*enable_features=*/MakeFeaturesString(enabled_features),
    /*disable_features=*/MakeFeaturesString(disabled_features));
```
