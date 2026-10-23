---
name: ios/remove-unused-grd-resources-and-assets
description: When removing code or feature flags referencing IDR_ resources or IDS_ strings, delete definitions from all .grd/.grdp files (including branded variants) and delete orphaned screenshots
rubric: correctness
severity: should_fix
citations:
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 12. Delete unused strings or resources
    quote: 'If any string or resource has no more users in either C++ or Java: - Delete its definition in a `.grd(p)` file. - Delete orphaned resource files. - For an orphaned string `IDS_FOO`, delete its corresponding `IDS_FOO.png.sha1` file, which references a screenshot to help translators.'
  - path: agents/projects/code-health/accessibility-cleanup/references/automated_review.md
    heading: Automated Review
    quote: Are there any strings that are now unused in the code? If so, ensure that the strings have been removed along with their corresponding png sha1 files if such files exist.
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 12. **Delete unused strings or resources**
    quote: 'If any string or resource has no more users in either C++ or Java: - Delete its definition in a `.grd(p)` file. - Delete orphaned resource files. - For an orphaned string `IDS_FOO`, delete its corresponding `IDS_FOO.png.sha1` file, which references a screenshot to help translators.'
---

**Rule.** When removing dead code, retiring feature flags, or refactoring logic
that references resources (`IDR_*`) or localized strings (`IDS_*`), verify
whether the last reference across all platforms (in C++, Objective-C, Java, or
WebUI code) has been removed. If no callers remain, delete the resource
(`<structure>`, `<include>`) or message (`<message>`) definitions from all
corresponding `.grd` and `.grdp` files. For branded strings, ensure definitions
are deleted from both branded and unbranded counterpart files (such as
`*_chromium_strings.grd` and `*_google_chrome_strings.grd`). Additionally,
delete any orphaned asset files (such as `.png`) and associated translation
screenshot hash files (`.png.sha1`) located in the matching `_grd/` or `_grdp/`
directories.

**Why.** Leaving unused resource or string definitions in `.grd` or `.grdp`
files packages dead assets into compiled binary `.pak` files and triggers
unnecessary translation churn and costs for obsolete strings across dozens of
locales. Omitting definitions from counterpart branded files causes divergence
between Chromium and Google Chrome builds. Deleting orphaned asset files and
`.png.sha1` screenshot hash files prevents repository bloat and keeps
translation screenshot catalogs synchronized with active strings.

## Examples

Bad:

```cpp
// 1. Removing IDR_ resource caller but leaving .grd entry and asset file:
- rb.GetNativeImageNamed(IDR_IOS_SETTINGS_INFO_24).ToUIImage();
// ios_theme_resources.grd and settings_info_24.png left on disk.

// 2. Removing localized string caller but leaving .grd entries and .png.sha1:
- l10n_util::GetNSString(IDS_IOS_SEGMENTED_DEFAULT_BROWSER_TITLE);
// Left in ios_chromium_strings.grd / ios_google_chrome_strings.grd,
// and IDS_IOS_SEGMENTED_DEFAULT_BROWSER_TITLE.png.sha1 left on disk.
```

Good:

```cpp
// 1. Delete IDR_ definition and remove asset from disk:
// ios/chrome/app/theme/ios_theme_resources.grd:
- <structure name="IDR_IOS_SETTINGS_INFO_24" file="settings_info_24.png" type="chrome_scaled_image" />
// git rm ios/chrome/app/theme/default_100_percent/settings_info_24.png

// 2. Delete IDS_ message from all matching .grd(p) files and remove .png.sha1:
// ios_chromium_strings.grd and ios_google_chrome_strings.grd:
- <message name="IDS_IOS_SEGMENTED_DEFAULT_BROWSER_TITLE" desc="...">
// git rm ios/chrome/app/strings/*_grd/IDS_IOS_SEGMENTED_DEFAULT_BROWSER_TITLE.png.sha1
```
