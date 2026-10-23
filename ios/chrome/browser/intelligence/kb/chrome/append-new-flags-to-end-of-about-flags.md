---
name: ios/append-new-flags-to-end-of-about-flags
description: When adding feature flags to `kFeatureEntries` in `about_flags`, append new entries to the end of the array rather than grouping or alphabetizing.
rubric: readability
severity: should_fix
citations:
  - path: docs/how_to_add_your_feature_flag.md
    heading: 'Step 2: Adding the feature flag to the chrome://flags UI.'
    quote: Add your changes at the bottom of the list, search for "Add new entries above this line."
---

**Rule.** When adding a new feature flag, experiment entry, or switch to
`kFeatureEntries` in `about_flags` (`about_flags.mm` or `about_flags.cc`),
append the entry to the end of the array directly above the closing array
bracket or the 'Add new entries above this line' comment. Do not sort entries
alphabetically or group them next to related features in the middle of the
array.

**Why.** Entries in `kFeatureEntries` are neither alphabetized nor grouped by
feature; they are intentionally appended in chronological order at the bottom to
minimize merge conflicts between concurrent branches modifying `about_flags`.
Unlike `flag_descriptions.h` (which is sorted alphabetically), `kFeatureEntries`
relies on appending to the end as documented in developer guides and source
comments.

## Examples

Bad:

```cpp
{"fullscreen-refactoring", flag_descriptions::kFullscreenRefactoringName,
     flag_descriptions::kFullscreenRefactoringDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFullscreenRefactoring)},
    // Bad: Inserted into the middle of kFeatureEntries to group or alphabetize.
    {"gemini-chat-persistence", flag_descriptions::kGeminiChatPersistenceName,
     flag_descriptions::kGeminiChatPersistenceDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kGeminiChatPersistence,
                                    kGeminiChatVariations,
                                    "GeminiChatPersistence")},
    {"ios-mini-map-universal-links",
     flag_descriptions::kIOSMiniMapUniversalLinkName,
     flag_descriptions::kIOSMiniMapUniversalLinkDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSMiniMapUniversalLink)},
```

Good:

```cpp
{"fullscreen-refactoring", flag_descriptions::kFullscreenRefactoringName,
     flag_descriptions::kFullscreenRefactoringDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFullscreenRefactoring)},
    // ...
    // Good: Appended at the end of kFeatureEntries above the closing bracket.
    {"ios-mini-map-universal-links",
     flag_descriptions::kIOSMiniMapUniversalLinkName,
     flag_descriptions::kIOSMiniMapUniversalLinkDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSMiniMapUniversalLink)},
    {"gemini-chat-persistence", flag_descriptions::kGeminiChatPersistenceName,
     flag_descriptions::kGeminiChatPersistenceDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kGeminiChatPersistence,
                                    kGeminiChatVariations,
                                    "GeminiChatPersistence")},
};
```
