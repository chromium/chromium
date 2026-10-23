---
name: ios/clean-up-feature-declarations-on-flag-removal
description: When removing feature flags or enabling features permanently, delete flag declarations, parameters, metadata, and about_flags entries, inline checks, and prune unreferenced downstream classes and tests
rubric: readability
severity: should_fix
citations:
  - path: docs/flag_expiry.md
    heading: Removing A Flag
    quote: If a flag is no longer used (for instance, it was used to control a feature that has since launched), the flag should be removed. Delete the entry in [`//chrome/browser/about_flags.cc`](../chrome/browser/about_flags.cc) or [`//ios/chrome/browser/flags/about_flags.mm`](../ios/chrome/browser/flags/about_flags.mm) for iOS (and any corresponding entries for the flag description), and remove any references in [`//chrome/browser/flag-metadata.json`](../chrome/browser/flag-metadata.json).
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 02. **Remove C++ flag enablement checks**
    quote: Find C++ callsites where the feature enablement is checked. This usually takes the form of `base::FeatureList::IsEnabled(kFeature)` calls or a free `Is<X>Enabled()` helper function that wraps the former. Simplify each enablement check as if the default state were inlined.
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 07. **Simplify APIs and expressions**
    quote: 'Inline trivial local variable assignments. For example, `const auto x = base::FeatureList::IsEnabled(kFoo) ? Enum::A : Enum::B; Bar(x);` ... will become ... `Bar(Enum::A);`'
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 08. **Remove dead code**
    quote: Remove declarations and definitions for the obsolete flag and its parameters. In all removed lines, check for functions, classes, and constants that are no longer referenced and delete their declarations and definitions. You may need to delete entire source files. - Continue this process for all code that becomes unreferenced.
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 14. **Update comments**
    quote: Check immediately above each modified code block for obsolete comments. If a comment still partially applies, reword it. Otherwise, delete it. Search the codebase for references to the feature flag name in other comments (e.g., both the C++ constant `kMyFeature` and the user-facing name `MyFeature`) to clean up obsolete discussions, TODOs, or design notes.
---

**Rule.** When removing an obsolete feature flag, field trial, or enabling a
feature permanently:

1. **Call sites and inlining**: Audit and eliminate all residual flag checks
   (`base::FeatureList::IsEnabled(...)` or `Is<Feature>Enabled()` helper
   accessors), conditional branches, and ternary fallbacks across call sites,
   inlining the enabled value, parameter, or permanent behavior directly.
2. **Prune downstream dead code**: Trace the entire removed execution flow and
   call graph downstream. Delete all classes, UI components (such as
   coordinators, mediators, and controllers), helper methods, and unit or
   integration tests that become completely unreferenced. Remove deleted files
   and obsolete dependencies from `BUILD.gn`.
3. **Declarations and definitions**: Delete the `BASE_DECLARE_FEATURE` header
   declaration, `BASE_FEATURE` definition, companion flags (e.g.,
   `*AllLocales`), associated parameters (`BASE_FEATURE_PARAM`,
   `BASE_DECLARE_FEATURE_PARAM`), helper accessor functions, and test overrides.
4. **Flag metadata and configuration**: In the same change, delete corresponding
   entries from `about_flags.mm` (or `about_flags.cc`), `flag_descriptions`,
   `flag-metadata.json`, and any entry in `flag-never-expire-list.json`.
5. **Comments and tests**: Remove or update obsolete comments, TODOs, and test
   descriptions referencing the removed flag or qualifying behavior with
   conditional clauses (such as 'Like above' or 'when [feature] is enabled').

**Why.** Incomplete feature flag cleanup leaves dead code, stale ternary
fallbacks, orphaned symbols, and misleading configurations across the codebase.
Deleting flags only from `about_flags` while retaining `BASE_FEATURE`
definitions, companion flags, or parameters clutters metadata tracking files
(`flag-metadata.json`) and symbol tables. Removing flag checks without tracing
downstream call paths leaves dead classes (such as orphaned coordinators and
mediators), obsolete helper methods, unreferenced tests, and stale `BUILD.gn`
dependencies in the repository. Fully pruning the dead execution path, deleting
declarations and metadata entries in the same change, inlining permanent
behavior at call sites, and updating stale comments ensures code runs
unconditionally as intended, reduces binary size, and prevents code rot.

## Examples

Bad:

```cpp
// Variant 1: Residual flag check and ternary fallback left at call site:
std::u16string maybeAnchorTags =
    IsPageActionMenuEnabled() ? kAnchorTagsJavaScript : u"";
ExecuteScript(maybeAnchorTags);

// Variant 2: Flag removed from UI, but declarations and metadata left behind:
// features.h / features.mm:
BASE_DECLARE_FEATURE(kWebPageAlternativeTextZoom);
BASE_FEATURE(kWebPageAlternativeTextZoom, base::FEATURE_ENABLED_BY_DEFAULT);
// about_flags.mm and flag-metadata.json still retain kWebPageAlternativeTextZoom.

// Variant 3: Downstream classes and UI components orphaned when flag is removed:
- (void)startGeminiFlowWithStartupState:(GeminiStartupState*)startupState {
  geminiBrowserAgent->StartGeminiFlow(self.viewController, startupState);
}
// Anti-pattern: LegacyGeminiCoordinator, LegacyGeminiMediator, unreferenced
// unit tests, and BUILD.gn entries are left orphaned behind in the codebase.
```

Good:

```cpp
// Variant 1: Flag check removed and permanent value inlined directly:
ExecuteScript(kAnchorTagsJavaScript);

// Variant 2: Delete declarations, companion flags, helpers, and metadata:
// (Removed BASE_DECLARE_FEATURE, BASE_FEATURE, and IsWebPageAlternativeTextZoomEnabled();
// removed entries from about_flags.mm, flag_descriptions, and flag-metadata.json)

// Variant 3: Downstream execution flow and dead classes pruned completely:
- (void)startGeminiFlowWithStartupState:(GeminiStartupState*)startupState {
  geminiBrowserAgent->StartGeminiFlow(self.viewController, startupState);
}
// Trace unreferenced call paths: delete legacy_gemini_coordinator.*, legacy_gemini_mediator.*,
// obsolete GeminiBrowserAgent methods, dead tests, and BUILD.gn entries.
```
