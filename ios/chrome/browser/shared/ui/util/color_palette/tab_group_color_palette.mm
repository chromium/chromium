// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/color_palette/tab_group_color_palette.h"

#import "base/containers/fixed_flat_map.h"
#import "base/notreached.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/ui/util/color_palette/color_palette_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

using TabGroupColorId = tab_groups::TabGroupColorId;

namespace {

struct ColorGroup {
  int tone05;
  int tone10;
  int tone20;
  int tone30;
  int tone70;
  int tone80;
  int tone90;
  int tone95;
};

static constexpr auto kColorGroupMap =
    base::MakeFixedFlatMap<tab_groups::TabGroupColorId, ColorGroup>({
        {tab_groups::TabGroupColorId::kGrey,
         {.tone05 = 0x0E1412,
          .tone10 = 0x191D1C,
          .tone20 = 0x303030,
          .tone30 = 0x444746,
          .tone70 = 0xA9ACAA,
          .tone80 = 0xC6C8C7,
          .tone90 = 0xE3E3E3,
          .tone95 = 0xF2F2F2}},

        {tab_groups::TabGroupColorId::kBlue,
         {.tone05 = 0x000F2E,
          .tone10 = 0x021942,
          .tone20 = 0x062D6B,
          .tone30 = 0x0F419A,
          .tone70 = 0x7AACF9,
          .tone80 = 0xA6C8FB,
          .tone90 = 0xD1E4FD,
          .tone95 = 0xE7F2FE}},

        {tab_groups::TabGroupColorId::kRed,
         {.tone05 = 0x270505,
          .tone10 = 0x390B09,
          .tone20 = 0x5E1812,
          .tone30 = 0x88201B,
          .tone70 = 0xFC8C86,
          .tone80 = 0xFDB4B1,
          .tone90 = 0xFEDBDC,
          .tone95 = 0xFEECEE}},

        {tab_groups::TabGroupColorId::kYellow,
         {.tone05 = 0x1F0C01,
          .tone10 = 0x2E1503,
          .tone20 = 0x4C2707,
          .tone30 = 0x6B3B10,
          .tone70 = 0xEC9932,
          .tone80 = 0xF5BD5E,
          .tone90 = 0xFEE089,
          .tone95 = 0xFEF2BA}},

        {tab_groups::TabGroupColorId::kGreen,
         {.tone05 = 0x011609,
          .tone10 = 0x022111,
          .tone20 = 0x053721,
          .tone30 = 0x0A5130,
          .tone70 = 0x4DC06F,
          .tone80 = 0x87D797,
          .tone90 = 0xC0EEBF,
          .tone95 = 0xDEF7DB}},

        {tab_groups::TabGroupColorId::kPink,
         {.tone05 = 0x2A0018,
          .tone10 = 0x3C0322,
          .tone20 = 0x600C37,
          .tone30 = 0x8A1051,
          .tone70 = 0xFC82CE,
          .tone80 = 0xFDAEDE,
          .tone90 = 0xFED9EE,
          .tone95 = 0xFEECF5}},

        {tab_groups::TabGroupColorId::kPurple,
         {.tone05 = 0x1C003C,
          .tone10 = 0x280652,
          .tone20 = 0x40127F,
          .tone30 = 0x562D9E,
          .tone70 = 0xC499F9,
          .tone80 = 0xD9BBFB,
          .tone90 = 0xEDDDFC,
          .tone95 = 0xF7ECFD}},

        {tab_groups::TabGroupColorId::kCyan,
         {.tone05 = 0x01141F,
          .tone10 = 0x021F2C,
          .tone20 = 0x053547,
          .tone30 = 0x0B4D66,
          .tone70 = 0x28BAE6,
          .tone80 = 0x6CD3F2,
          .tone90 = 0xAFECFD,
          .tone95 = 0xD9F6FE}},

        {tab_groups::TabGroupColorId::kOrange,
         {.tone05 = 0x210B00,
          .tone10 = 0x311303,
          .tone20 = 0x512409,
          .tone30 = 0x733610,
          .tone70 = 0xFC8F4F,
          .tone80 = 0xFDB68B,
          .tone90 = 0xFEDDC6,
          .tone95 = 0xFEEDE2}},
    });

static_assert(kColorGroupMap.size() ==
                  static_cast<size_t>(TabGroupColorId::kNumEntries),
              "");

// The transparency for the bars.
const float kLightBarToneAlpha = 0.3f;
const float kDarkBarToneAlpha = 0.5f;

// Creates a DynamicProvider for light/dark colors.
UIColor* CreateDynamicProviderFromRGB(int lightColor,
                                      int darkColor,
                                      CGFloat lightAlpha = 1.0,
                                      CGFloat darkAlpha = 1.0) {
  return [UIColor
      colorWithDynamicProvider:GetDynamicProvider(FromColor(UIColorFromRGB(
                                                      lightColor, lightAlpha)),
                                                  FromColor(UIColorFromRGB(
                                                      darkColor, darkAlpha)))];
}

}  // namespace

@implementation TabGroupColorPalette

+ (UIColor*)commonColor:(TabGroupColorId)tabGroupColorID {
  const ColorGroup& group = kColorGroupMap.at(tabGroupColorID);

  return UIColorFromRGB(group.tone70);
}

+ (NSArray<UIColor*>*)gradientBackgroundColors:
    (tab_groups::TabGroupColorId)tabGroupColorID {
  const ColorGroup& group = kColorGroupMap.at(tabGroupColorID);

  return @[
    CreateDynamicProviderFromRGB(group.tone95, group.tone05),
    CreateDynamicProviderFromRGB(group.tone80, group.tone20),
    CreateDynamicProviderFromRGB(group.tone70, group.tone30),
  ];
}

- (instancetype)initWithColorId:(TabGroupColorId)tabGroupColorID {
  self = [super init];
  if (self) {
    _tabGroupColorID = tabGroupColorID;
    const ColorGroup& group = kColorGroupMap.at(tabGroupColorID);

    _backgroundColor = CreateDynamicProviderFromRGB(group.tone95, group.tone30);
    _snapshotBackgroundColor =
        CreateDynamicProviderFromRGB(group.tone90, group.tone10);
    _barColor = CreateDynamicProviderFromRGB(
        group.tone70, group.tone30, kLightBarToneAlpha, kDarkBarToneAlpha);
    _commonColor = UIColorFromRGB(group.tone70);
  }

  return self;
}

@end
