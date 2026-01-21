// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/util/tab_group_color_palette.h"

#import <CoreGraphics/CGColor.h>
#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/ui/util/color_palette/color_palette_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "skia/ext/skia_utils_ios.h"
#import "third_party/skia/include/core/SkColor.h"
#import "ui/color/dynamic_color/palette_factory.h"

namespace {
// The tone for the cells' background.
const int kBackgroundToneLight = 95;
const int kBackgroundToneDark = 30;

// The tone for the snaphots' background.
// This tone is visible when the thumbnail is empty.
const int kSnapshotBackgroundToneLight = 90;
const int kSnapshotBackgroundToneDark = 10;

// The bars' tone visible when the thumbnail is empty.
const int kBarToneLight = 70;
const int kBarToneDark = 30;
// The transparency for the bars.
const float kBarToneAlpha = 0.3f;

// The tone for the border and the dot, in both light and dark mode.
const int kCommonTone = 70;

// Normalize color to sRGB space.
UIColor* SRGBColorFromColor(UIColor* color) {
  // Create the standard sRGB color space.
  UIColor* wideColor = color;
  CGColorSpaceRef sRGB = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);

  // Convert the underlying CGColor to sRGB.
  CGColorRef sRGBColorRef = CGColorCreateCopyByMatchingToColorSpace(
      sRGB, kCGRenderingIntentDefault, wideColor.CGColor, nullptr);

  // Wrap it back in UIColor.
  UIColor* standardColor = [UIColor colorWithCGColor:sRGBColorRef];

  // Clean up.
  CGColorSpaceRelease(sRGB);
  CGColorRelease(sRGBColorRef);

  return standardColor;
}

}  // namespace

@implementation TabGroupColorPalette

- (instancetype)initWithSeedColor:(UIColor*)groupColor {
  CHECK(groupColor);

  self = [super init];
  if (self) {
    UITraitCollection* lightTraitCollection = [UITraitCollection
        traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleLight];
    UIColor* lightGroupColor =
        [groupColor resolvedColorWithTraitCollection:lightTraitCollection];

    UIColor* standardColor = SRGBColorFromColor(lightGroupColor);

    std::unique_ptr<ui::Palette> palette =
        ui::GeneratePalette(skia::UIColorToSkColor(standardColor),
                            ui::ColorProviderKey::SchemeVariant::kTonalSpot);
    const ui::TonalPalette& primary = palette->primary();

    _seedColor = standardColor;

    _backgroundColor = [UIColor
        colorWithDynamicProvider:GetDynamicProvider(
                                     FromTone(PaletteTone(
                                         primary, kBackgroundToneLight)),
                                     FromTone(PaletteTone(
                                         primary, kBackgroundToneDark)))];

    _snapshotBackgroundColor = [UIColor
        colorWithDynamicProvider:GetDynamicProvider(
                                     FromTone(PaletteTone(
                                         primary,
                                         kSnapshotBackgroundToneLight)),
                                     FromTone(PaletteTone(
                                         primary,
                                         kSnapshotBackgroundToneDark)))];

    _barColor =
        [[UIColor colorWithDynamicProvider:GetDynamicProvider(
                                               FromTone(PaletteTone(
                                                   primary, kBarToneLight)),
                                               FromTone(PaletteTone(
                                                   primary, kBarToneDark)))]
            colorWithAlphaComponent:kBarToneAlpha];

    _commonColor =
        [UIColor colorWithDynamicProvider:GetDynamicProvider(
                                              FromTone(PaletteTone(
                                                  primary, kCommonTone)),
                                              FromTone(PaletteTone(
                                                  primary, kCommonTone)))];
  }

  return self;
}

@end
