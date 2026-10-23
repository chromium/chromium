---
name: ios/avoid-feature-specific-names-for-shared-constants
description: When declaring or generalizing functions, parameters, constants, enums, or properties in shared utilities or reusable components, name them after intrinsic roles rather than specific features
rubric: readability
severity: should_fix
---

**Rule.** Name every entity after its actual scope, not after the caller,
feature, surface, sub-feature, subtype, or lifecycle phase that happened to
introduce it. Do not include caller-, feature-, or surface-specific terms (such
as `GeminiLive`, `Promo`, `Latency`, or `gridCells`) in the names of shared
functions, parameters, selector segments, constants, enum values, resource
assets, histogram tokens, or generic properties; when defining or generalizing a
component, utility, or animation across multiple features, name functions,
parameters, enum constants, and properties after their intrinsic content,
destination, domain-agnostic target entity, or abstract role (such as
`Instructions` rather than `Promo`, `targetViews` rather than `gridCells`, or
`kRevealTarget`). Likewise, when a class, UI element, method, callback,
property, or capacity limit grows beyond its original specialized purpose,
rename it to reflect its generalized role (`IOSTracingController` instead of
`IOSStartupTracingController`, `shouldShowFeatureEntryPoints` instead of
`shouldShowAIEntryPoints` once it also gates non-AI features) rather than
documenting around the mismatch with comments. If renaming immediately across
many files would cause widespread churn during a targeted fix, track the rename
with a `TODO(crbug.com/...)` referencing a tracking bug.

**Why.** Feature-, caller-, or phase-specific names in shared utilities,
reusable animations, generic components, or widened classes break abstraction,
misrepresent the entity's scope, and mislead readers into assuming the code is
coupled to a single caller or scenario, producing confusing call sites as
additional callers and generic variants are handled. Intrinsic, domain-agnostic
naming keeps functions, parameters, constants, enum values, properties, and
classes self-documenting and reusable, and an explicit tracking TODO ensures a
deferred rename is completed without blocking immediate delivery.

## Examples

Bad:

```cpp
// Bad: Naming a shared instructional helper after a promo caller:
void ShowDefaultBrowserPictureInPicturePromo(
    id<PictureInPictureCommands> handler);

// Bad: Includes caller/feature 'Promo' for a shared video instructions asset:
NSString* kDefaultBrowserPromoSettingsDestinationVideo =
    @"default_browser_promo_settings_destination";

// Bad: Feature-specific parameter and enum in reusable animation component:
_tabsClosureAnimation =
    [[TabsClosureAnimation alloc] initWithWindow:self.view
                                       gridCells:@[ _contentView ]];
_tabsClosureAnimation.type = TabsClosureAnimationType::kRevealGridCells;

// Bad: Hardcoding a feature-specific constant in a generic UI component:
const CGFloat kGeminiLiveCircleSize = 20.0;

// Bad: Class manages general tracing beyond startup, but is named narrowly:
class IOSStartupTracingController {
  perfetto::TraceConfig CreateDeveloperTraceConfig();
};

// Bad: Method controls multiple features (including Reader Mode), not just AI:
- (BOOL)shouldShowAIEntryPoints {
  return [self isUserSignedIn];
}
```

Good:

```cpp
// Good: Naming the helper after its intrinsic instructional role:
void ShowDefaultBrowserPictureInPictureInstructions(
    id<PictureInPictureCommands> handler);

// Good: Names constant and asset after intrinsic content and destination:
NSString* kAppSpecificSettingsInstructionsVideo =
    @"app_specific_settings_instructions_video";

// Good: Generic parameter and enum constant describing the target entities:
_radialWipeAnimation =
    [[RadialWipeAnimation alloc] initWithWindow:self.view
                                    targetViews:@[ _contentView ]];
_radialWipeAnimation.type = RadialWipeAnimationType::kRevealTarget;

// Good: Generic property reflecting the abstract layout role:
CGFloat _customLeadingViewTargetWidth;

// Good: Named to reflect its full scope of responsibility:
class IOSTracingController {
  perfetto::TraceConfig CreateDeveloperTraceConfig();
};

// Good: Umbrella identifier covering all managed features:
- (BOOL)shouldShowFeatureEntryPoints {
  return [self isUserSignedIn];
}

// Good: Rename deferred with a tracked TODO when immediate churn is too wide:
// TODO(crbug.com/123456): Rename to `NavigationItemTitleWithLogoView`.
@interface BrandedNavigationItemTitleView : UIView
@end
```
