// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/color_palette/tab_group_color_palette.h"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/ui/util/color_palette/color_palette_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "skia/ext/skia_utils_ios.h"
#import "third_party/skia/include/core/SkColor.h"
#import "ui/color/dynamic_color/palette_factory.h"

namespace {

// Hex values for seed colors.
int const kGreyColor = 0x747775;    // RGB: (0.45, 0.47, 0.46).
int const kBlueColor = 0x3271EA;    // RGB: (0.20, 0.44, 0.92).
int const kRedColor = 0xDC362E;     // RGB: (0.86, 0.21, 0.18).
int const kYellowColor = 0xB16300;  // RGB: (0.69, 0.39, 0.00).
int const kGreenColor = 0x128937;   // RGB: (0.07, 0.54, 0.22).
int const kPinkColor = 0xDC258D;    // RGB: (0.86, 0.15, 0.55).
int const kPurpleColor = 0x9254EA;  // RGB: (0.57, 0.33, 0.92).
int const kCyanColor = 0x0081A8;    // RGB: (0.00, 0.51, 0.66).
int const kOrangeColor = 0xC05A01;  // RGB: (0.75, 0.35, 0.00).

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

// Maps a tab group color id to the corresponding seed color.
UIColor* ColorForTabGroupColorId(
    tab_groups::TabGroupColorId tab_group_color_id) {
  switch (tab_group_color_id) {
    case tab_groups::TabGroupColorId::kGrey:
      return UIColorFromRGB(kGreyColor);
    case tab_groups::TabGroupColorId::kBlue:
      return UIColorFromRGB(kBlueColor);
    case tab_groups::TabGroupColorId::kRed:
      return UIColorFromRGB(kRedColor);
    case tab_groups::TabGroupColorId::kYellow:
      return UIColorFromRGB(kYellowColor);
    case tab_groups::TabGroupColorId::kGreen:
      return UIColorFromRGB(kGreenColor);
    case tab_groups::TabGroupColorId::kPink:
      return UIColorFromRGB(kPinkColor);
    case tab_groups::TabGroupColorId::kPurple:
      return UIColorFromRGB(kPurpleColor);
    case tab_groups::TabGroupColorId::kCyan:
      return UIColorFromRGB(kCyanColor);
    case tab_groups::TabGroupColorId::kOrange:
      return UIColorFromRGB(kOrangeColor);
    case tab_groups::TabGroupColorId::kNumEntries:
      NOTREACHED() << "kNumEntries is not a supported color enum.";
  }
}

}  // namespace

@implementation TabGroupColorPalette

- (instancetype)initWithSeedColorId:
    (tab_groups::TabGroupColorId)tabGroupColorId {
  self = [super init];
  if (self) {
    _seedColor = ColorForTabGroupColorId(tabGroupColorId);

    std::unique_ptr<ui::Palette> palette =
        ui::GeneratePalette(skia::UIColorToSkColor(_seedColor),
                            ui::ColorProviderKey::SchemeVariant::kTonalSpot);
    const ui::TonalPalette& primary = palette->primary();

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
