---
name: ios/use-subsystem-presentation-helpers
description: When determining UI layout, presentation, sizing, or preconditions, inspect trait collections and presentation helpers instead of asserting or querying physical hardware form factors
rubric: correctness
severity: blocking
---

**Rule.** Do not inspect or assert physical hardware device form factors or
idioms (such as `ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET`,
`CHECK_NE(ui::GetDeviceFormFactor(), ui::DEVICE_FORM_FACTOR_TABLET)`,
`UIDevice.currentDevice.userInterfaceIdiom`, or
`[UIDevice currentDevice].userInterfaceIdiom`) to determine UI presentation
styles, layout sizing, viewport geometry, fallback behavior, margins/padding, or
code path preconditions. Instead, adapt to the dynamic layout environment:

1. For adaptive layout, sizing, and presentation styles, inspect
   `UITraitCollection` size classes (`horizontalSizeClass`,
   `IsRegularXRegularSizeClass`) or use subsystem presentation helpers and
   configured mode enums (such as `ResultPagePresentationFor` or
   `ComposeboxInputPlatePosition`).
2. For layout spacing, margins, or padding, query dynamic view layout metrics
   directly (such as `safeAreaInsets`) rather than hardcoding offsets based on
   device form factor.
3. In code paths handling compact traits or tab-strip-hidden states
   (`!CanShowTabStrip(...)`), never assert non-tablet hardware. Gate
   preconditions and visibility on trait environments or layout helpers rather
   than hardware form factors.

**Why.** Hardware form factor and device idiom queries reflect physical device
hardware rather than the active window geometry or presentation environment. On
iPad, multitasking configurations (Split View, Slide Over, and Stage Manager)
resize windows into compact size classes where `CanShowTabStrip(...)` returns
false and phone-like layouts are used; asserting non-tablet hardware
(`CHECK_NE`) in compact UI paths causes fatal runtime crashes, and branching on
device form factor produces broken layouts. Furthermore, physical device idiom
checks on iPhone fail to account for differing safe-area geometries (such as
devices with home indicators vs. button-based devices like iPhone SE).
Inspecting dynamic layout traits, size classes, `safeAreaInsets`, or subsystem
presentation helpers ensures views handle multitasking, orientation changes, and
distinct safe areas safely and accurately.

## Examples

Bad:

```cpp
// Variant 1: Asserting non-tablet hardware in compact UI paths
- (ToolbarProgressBar*)createProgressBar {
  CHECK_NE(ui::GetDeviceFormFactor(), ui::DEVICE_FORM_FACTOR_TABLET);
  return [[ToolbarProgressBar alloc] init];
}

// Variant 2: Branching on hardware form factor or idiom for sizing / padding
- (CGSize)viewportSize {
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
             ? kViewportSizeIPad
             : kViewportSizeIPhone;
}
CGFloat bottomPadding =
    ([UIDevice currentDevice].userInterfaceIdiom == UIUserInterfaceIdiomPad)
        ? kVerticalListHorizontalPadding
        : 0;
```

Good:

```cpp
// Variant 1: Gating on layout environment without non-tablet assertions
- (ToolbarProgressBar*)createProgressBar {
  return [[ToolbarProgressBar alloc] init];
}

// Variant 2: Inspecting UITraitCollection and dynamic safeAreaInsets
- (CGSize)viewportSize {
  return self.traitCollection.horizontalSizeClass ==
                 UIUserInterfaceSizeClassCompact
             ? kViewportSizeIPhone
             : kViewportSizeIPad;
}
const CGFloat bottomSafeArea = _viewController.view.safeAreaInsets.bottom;
const CGFloat bottomPadding =
    (bottomSafeArea >= kVerticalListHorizontalPadding)
        ? 0
        : (kVerticalListHorizontalPadding - bottomSafeArea);
```
