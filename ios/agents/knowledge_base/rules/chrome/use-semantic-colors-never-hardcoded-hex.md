---
name: ios/use-semantic-colors-never-hardcoded-hex
description: Use shared semantic colors from semantic_color_names.h and //ios/chrome/common/ui/colors instead of hardcoded RGB/hex values, UIKit system colors, or local .colorset assets
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: UI and Assets Best Practices
    quote: Never use standard UIKit system colors (such as `UIColor.redColor` or `UIColor.systemBlueColor`; only `whiteColor`, `blackColor`, and `clearColor` are permitted) or hardcoded RGB values. Always use semantic color names from `//ios/chrome/common/ui/colors/semantic_color_names.h` via `[UIColor colorNamed:k...]` so Light and Dark Mode work automatically.
  - path: ios/architecture/mediators.md
    heading: Data Types Passed Over Consumer Protocols
    quote: Pass `UIColor*` values over a consumer protocol only when the color is intrinsically associated with a specific model instance (for example, a tab group color or a color provided by a cross-platform model API).
---

**Rule.** Use semantic colors from
`ios/chrome/common/ui/colors/semantic_color_names.h` via
`[UIColor colorNamed:k...]` across iOS Chrome views, view controllers, and
controls. Do not hardcode RGB or hex values, and do not use UIKit system or
named colors (only `whiteColor`, `blackColor`, and `clearColor` are permitted).
Add new `.colorset` assets under `ios/chrome/common/ui/colors/` and depend on
`//ios/chrome/common/ui/colors` instead of defining local `.colorset` catalogs
in `BUILD.gn`. Colors that come from model data rather than a design choice
(such as a user-selected theme color from a cross-platform model API, converted
with `skia::UIColorFromSkColor()`) may be passed as `UIColor*` over the consumer
protocol.

**Why.** Semantic colors in `semantic_color_names.h` are backed by shared
`.colorset` assets in `ios/chrome/common/ui/colors/` that adapt automatically to
Light and Dark Mode. Hardcoded RGB or hex values, UIKit system colors, and local
`.colorset` catalogs bypass the shared palette and risk broken contrast. See
`ios/best_practices.md#ui-and-assets-best-practices` and
`ios/architecture/mediators.md#data-types-passed-over-consumer-protocols`.

## Examples

Bad:

```cpp
// 1. UIKit system/named colors and hardcoded RGB/hex values:
view.backgroundColor = UIColor.systemBlueColor;
badge.backgroundColor =
    [UIColor colorWithRed:0.0 green:0.478 blue:1.0 alpha:1.0];
separator.backgroundColor = UIColor.lightGrayColor;

// 2. UIKit system colors in button configurations:
UIButtonConfiguration* config = self.actionButton.configuration;
config.image = iconImage;
config.imageColorTransformer = ^UIColor*(UIColor* color) {
  return UIColor.redColor;
};
self.actionButton.configuration = config;

// 3. Local .colorset asset outside //ios/chrome/common/ui/colors/ (BUILD.gn):
bundle_data_xcassets("resources") {
  catalog = "Assets.xcassets"
  sources = [ "Assets.xcassets/accent_color.colorset/Contents.json" ]
}
```

Good:

```cpp
// 1. Shared semantic colors adapt across Light and Dark Mode:
view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
badge.backgroundColor = [UIColor colorNamed:kBlue300Color];
separator.backgroundColor = [UIColor colorNamed:kGrey300Color];

// 2. Semantic colors in button configurations:
UIButtonConfiguration* config = self.actionButton.configuration;
config.image = iconImage;
config.imageColorTransformer = ^UIColor*(UIColor* color) {
  return [UIColor colorNamed:kSolidButtonTextColor];
};
self.actionButton.configuration = config;

// 3. Register new .colorset assets in //ios/chrome/common/ui/colors/ and
// depend on it in BUILD.gn:
source_set("ui") {
  sources = [ "feature_view.mm" ]
  deps = [ "//ios/chrome/common/ui/colors" ]
}
```
