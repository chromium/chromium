---
name: ios/use-semantic-colors-never-hardcoded-hex
description: In iOS UI code and BUILD.gn asset catalogs, use shared semantic colors from semantic_color_names.h and //ios/chrome/common/ui/colors instead of hardcoded RGB/hex values, UIKit system colors, or duplicate .colorset assets
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: UI and Assets Best Practices
    quote: Never use standard UIKit system colors (such as `UIColor.redColor` or `UIColor.systemBlueColor`; only `whiteColor`, `blackColor`, and `clearColor` are permitted) or hardcoded RGB values. Always use semantic color names from `//ios/chrome/common/ui/colors/semantic_color_names.h` via `[UIColor colorNamed:k...]`.
---

**Rule.** In iOS UI views, view controllers, and controls, use semantic color
names from `ios/chrome/common/ui/colors/semantic_color_names.h` via
`[UIColor colorNamed:k...]` (such as `kBackgroundColor`,
`kPrimaryBackgroundColor`, `kBlue300Color`, or button tokens like
`kSolidButtonTextColor`) rather than standard UIKit system colors (such as
`UIColor.redColor`, `UIColor.systemBlueColor`, or `UIColor.lightGrayColor`) or
hardcoded RGB values (`[UIColor colorWithRed:green:blue:alpha:]` or
`[UIColor colorWithWhite:alpha:]`); only `UIColor.whiteColor`,
`UIColor.blackColor`, and `UIColor.clearColor` are permitted as static presets.
Apply the same rule when configuring button text, icons, or foreground content
(via `UIButtonConfiguration`, `imageColorTransformer`,
`titleTextAttributesTransformer`, or `setTitleColor:`). Avoid manually
inspecting `UITraitCollection.userInterfaceStyle` or constructing custom
`[UIColor colorWithDynamicProvider:]` blocks when an existing semantic colorset
(such as `kSolidWhiteColor` or `kSolidBlackColor`) satisfies the light and dark
requirements. When a new color is needed, add a `.colorset` asset under
`ios/chrome/common/ui/colors/` and a matching name in `semantic_color_names.h`
instead of defining the color in code; the one exception is a color that is
intrinsically part of a model instance (such as a tab group color), which the
mediator passes as a `UIColor*` over the consumer protocol. Do not redeclare
standard Chrome UI `.colorset` assets in a feature's or extension's
`bundle_data_xcassets` target; depend on `//ios/chrome/common/ui/colors` and
keep only feature-specific assets in a local catalog.

**Why.** Standard UIKit system colors and hardcoded RGB values are not part of
Chrome's themed palette and do not adapt the way the semantic colorsets do
across Light Mode, Dark Mode, and Incognito, causing low contrast, inverted
appearances, or illegible controls and text against themed backgrounds. Semantic
colors in `semantic_color_names.h` (including button tokens like
`kSolidButtonTextColor`) are backed by asset catalog colorsets that resolve to
the appropriate light or dark variant for the active trait collection, so
defining colors in code bypasses the central palette and prevents app-wide theme
updates. Manually branching on `userInterfaceStyle` or creating custom dynamic
provider blocks adds redundant logic and risks theme desynchronization.
`whiteColor`, `blackColor`, and `clearColor` are permitted because they are
appearance-invariant primitives rather than themed UI colors. See
`ios/best_practices.md#ui-and-assets-best-practices`.

## Examples

Bad:

```cpp
// Bad: UIKit system colors and hardcoded RGB bypass the semantic palette.
view.backgroundColor = UIColor.systemBlueColor;
badge.backgroundColor = [UIColor colorWithRed:0.0 green:0.478 blue:1.0 alpha:1.0];
separator.backgroundColor = UIColor.lightGrayColor;

// Bad: Static system colors in button configurations and transformers.
UIButtonConfiguration* config = self.primaryActionButton.configuration;
config.image = ArrowDownImage();
config.imageColorTransformer = ^UIColor*(UIColor* _) {
  return UIColor.redColor;
};
self.primaryActionButton.configuration = config;

// Bad: Redundant dynamic provider branching when a semantic color exists.
overlay.backgroundColor =
    [UIColor colorWithDynamicProvider:^UIColor*(UITraitCollection* traits) {
      return traits.userInterfaceStyle == UIUserInterfaceStyleDark
                 ? [UIColor.blackColor colorWithAlphaComponent:kAlpha]
                 : [UIColor.whiteColor colorWithAlphaComponent:kAlpha];
    }];

// Bad: Defining a new color in code instead of adding a .colorset asset.
UIColor* AccentColor() {
  return [UIColor colorWithRed:0.1 green:0.5 blue:0.9 alpha:1.0];
}
```

Good:

```cpp
// Good: Semantic colors adapt across light, dark, and incognito.
view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
badge.backgroundColor = [UIColor colorNamed:kBlue300Color];
separator.backgroundColor = [UIColor colorNamed:kGrey300Color];

// Good: Semantic colors preserve button foreground contrast.
UIButtonConfiguration* config = self.primaryActionButton.configuration;
config.image = ArrowDownImage();
config.imageColorTransformer = ^UIColor*(UIColor* _) {
  return [UIColor colorNamed:kSolidButtonTextColor];
};
self.primaryActionButton.configuration = config;

// Good: Predefined semantic color eliminates manual trait branching.
overlay.backgroundColor =
    [[UIColor colorNamed:kSolidWhiteColor] colorWithAlphaComponent:kAlpha];

// Good: New colors are added as .colorset assets and named in
// semantic_color_names.h.
view.tintColor = [UIColor colorNamed:kBlue600Color];
```
