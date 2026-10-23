---
name: ios/avoid-duplicate-feature-flag-cleanup-todos
description: When tracking feature flag cleanups or deprecations, place removal TODOs on header declarations rather than call sites, but annotate retained legacy methods, flows, and variables
rubric: readability
severity: should_fix
citations:
  - path: docs/ios/style.md
    heading: TODO comments
    quote: '* **Every TODO must have a bug** * Bug should be labeled with **Hotlist-TODO-iOS** * Always list bug in parentheses following "TODO" * `// TODO(crbug.com/######): Something that needs doing.`'
---

**Rule.** When tracking cleanup or removal for temporary feature flags, kill
switches, or deprecated APIs and methods, place the `TODO(crbug.com/<id>)` on
the declaration in the header (such as `BASE_DECLARE_FEATURE(...)` or the
interface/protocol method declaration), rather than duplicating TODOs across
internal call sites, definitions, or tests. Conversely, when retaining legacy
methods, obsolete flows, conditional branches, enum identifiers, or state
variables slated for deletion once the feature launches, annotate them with
`// TODO(crbug.com/<bug>): Remove once <Feature> is enabled by default.` to
ensure they are deleted during post-launch cleanup.

**Why.** Header and protocol declarations define the API contract and alert
callers not to adopt deprecated methods or temporary flags, while centralizing
cleanup TODOs on declarations avoids comment clutter across internal call sites
and implementations. Conversely, obsolete legacy flows, methods, fallback
branches, and decoupled enums or variables easily become untracked technical
debt if not explicitly marked with removal TODOs tied to the feature's launch.

## Examples

Bad:

```cpp
// Retaining deprecated protocol method without declaration TODO (annotated only in .mm):
@optional
- (void)didChangeLiveProcessStatus:(GeminiClientMode)status;

// Duplicating flag cleanup TODOs at call sites:
// TODO(crbug.com/123456): Remove once feature is enabled.
if (base::FeatureList::IsEnabled(features::kNewFeature)) { ... }

// Retaining obsolete legacy flow or decoupled enum without removal tracking:
ItemIdentifierPasswords,
- (void)setGroupColor:(UIColor*)groupColor {
  CHECK(!IsTabGroupColorOnSurfaceEnabled());
}
```

Good:

```cpp
// In header (single authoritative TODO on declaration):
// TODO(crbug.com/504596190): Remove once internal migration completes.
- (void)didChangeLiveProcessStatus:(GeminiClientMode)status;
// TODO(crbug.com/449156290): Clean up kill switch once launched.
BASE_DECLARE_FEATURE(kUpdateSSLStatus);

// Call sites need no duplicate TODOs:
if (base::FeatureList::IsEnabled(features::kNewFeature)) { ... }

// Retained legacy flow and decoupled enum annotated with removal tracking:
// TODO(crbug.com/463402932): Remove once feature is enabled by default.
ItemIdentifierPasswords,
// TODO(crbug.com/445887468): Remove this flow once IsTabGroupColorOnSurfaceEnabled is default-enabled.
- (void)setGroupColor:(UIColor*)groupColor {
  CHECK(!IsTabGroupColorOnSurfaceEnabled());
}
```
