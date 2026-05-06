// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/color_palette/tab_group_color_palette.h"

#import "base/containers/fixed_flat_map.h"
#import "base/notreached.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/color_palette/color_palette_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

using TabGroupColorId = tab_groups::TabGroupColorId;

namespace {

// The transparency for the bars.
constexpr float kLightBarToneAlpha = 0.3f;
constexpr float kDarkBarToneAlpha = 0.5f;

// This struct contains all the tones for a color group in either light or dark
// mode. Each field represents a specific tone from a palette (formatted as
// light/dark):
//  - common_tone : 70/70
//  - background_tone : 95/30
//  - snapshot_background_tone : 90/10
//  - bar_tone : 70/30
//  - first_gradient_tone : 95/05
//  - second_gradient_tone : 80/20
//  - third_gradient_tone : 70/30
struct ColorTones {
  int common_tone;
  int background_tone;
  int snapshot_background_tone;
  int bar_tone;
  int first_gradient_tone;
  int second_gradient_tone;
  int third_gradient_tone;
};

// The struct containing light and dark tones for each group color.
struct ColorGroup {
  ColorTones light;
  ColorTones dark;
};

// A map associating each `TabGroupColorId` with its ColorGroup tones.
static constexpr auto kColorGroupMap =
    base::MakeFixedFlatMap<tab_groups::TabGroupColorId, ColorGroup>({
        {tab_groups::TabGroupColorId::kGrey,
         ColorGroup{.light = {.common_tone = 0xA9ACAA,
                              .background_tone = 0xF2F2F2,
                              .snapshot_background_tone = 0xE3E3E3,
                              .bar_tone = 0xA9ACAA,
                              .first_gradient_tone = 0xF2F2F2,
                              .second_gradient_tone = 0xC6C8C7,
                              .third_gradient_tone = 0xA9ACAA},
                    .dark = {.common_tone = 0xA9ACAA,
                             .background_tone = 0x444746,
                             .snapshot_background_tone = 0x191D1C,
                             .bar_tone = 0x444746,
                             .first_gradient_tone = 0x0E1412,
                             .second_gradient_tone = 0x303030,
                             .third_gradient_tone = 0x444746}}},

        {tab_groups::TabGroupColorId::kBlue,
         ColorGroup{.light = {.common_tone = 0x7AACF9,
                              .background_tone = 0xE7F2FE,
                              .snapshot_background_tone = 0xD1E4FD,
                              .bar_tone = 0x7AACF9,
                              .first_gradient_tone = 0xE7F2FE,
                              .second_gradient_tone = 0xA6C8FB,
                              .third_gradient_tone = 0x7AACF9},
                    .dark = {.common_tone = 0x7AACF9,
                             .background_tone = 0x0F419A,
                             .snapshot_background_tone = 0x021942,
                             .bar_tone = 0x0F419A,
                             .first_gradient_tone = 0x000F2E,
                             .second_gradient_tone = 0x062D6B,
                             .third_gradient_tone = 0x0F419A}}},

        {tab_groups::TabGroupColorId::kRed,
         ColorGroup{.light = {.common_tone = 0xFC8C86,
                              .background_tone = 0xFEECEE,
                              .snapshot_background_tone = 0xFEDBDC,
                              .bar_tone = 0xFC8C86,
                              .first_gradient_tone = 0xFEECEE,
                              .second_gradient_tone = 0xFDB4B1,
                              .third_gradient_tone = 0xFC8C86},
                    .dark = {.common_tone = 0xFC8C86,
                             .background_tone = 0x88201B,
                             .snapshot_background_tone = 0x390B09,
                             .bar_tone = 0x88201B,
                             .first_gradient_tone = 0x270505,
                             .second_gradient_tone = 0x5E1812,
                             .third_gradient_tone = 0x88201B}}},

        {tab_groups::TabGroupColorId::kYellow,
         ColorGroup{.light = {.common_tone = 0xEC9932,
                              .background_tone = 0xFEF2BA,
                              .snapshot_background_tone = 0xFEE089,
                              .bar_tone = 0xEC9932,
                              .first_gradient_tone = 0xFEF2BA,
                              .second_gradient_tone = 0xF5BD5E,
                              .third_gradient_tone = 0xEC9932},
                    .dark = {.common_tone = 0xEC9932,
                             .background_tone = 0x6B3B10,
                             .snapshot_background_tone = 0x2E1503,
                             .bar_tone = 0x6B3B10,
                             .first_gradient_tone = 0x1F0C01,
                             .second_gradient_tone = 0x4C2707,
                             .third_gradient_tone = 0x6B3B10}}},

        {tab_groups::TabGroupColorId::kGreen,
         ColorGroup{.light = {.common_tone = 0x4DC06F,
                              .background_tone = 0xDEF7DB,
                              .snapshot_background_tone = 0xC0EEBF,
                              .bar_tone = 0x4DC06F,
                              .first_gradient_tone = 0xDEF7DB,
                              .second_gradient_tone = 0x87D797,
                              .third_gradient_tone = 0x4DC06F},
                    .dark = {.common_tone = 0x4DC06F,
                             .background_tone = 0x0A5130,
                             .snapshot_background_tone = 0x022111,
                             .bar_tone = 0x0A5130,
                             .first_gradient_tone = 0x011609,
                             .second_gradient_tone = 0x053721,
                             .third_gradient_tone = 0x0A5130}}},

        {tab_groups::TabGroupColorId::kPink,
         ColorGroup{.light = {.common_tone = 0xFC82CE,
                              .background_tone = 0xFEECF5,
                              .snapshot_background_tone = 0xFED9EE,
                              .bar_tone = 0xFC82CE,
                              .first_gradient_tone = 0xFEECF5,
                              .second_gradient_tone = 0xFDAEDE,
                              .third_gradient_tone = 0xFC82CE},
                    .dark = {.common_tone = 0xFC82CE,
                             .background_tone = 0x8A1051,
                             .snapshot_background_tone = 0x3C0322,
                             .bar_tone = 0x8A1051,
                             .first_gradient_tone = 0x2A0018,
                             .second_gradient_tone = 0x600C37,
                             .third_gradient_tone = 0x8A1051}}},

        {tab_groups::TabGroupColorId::kPurple,
         ColorGroup{.light = {.common_tone = 0xC499F9,
                              .background_tone = 0xF7ECFD,
                              .snapshot_background_tone = 0xEDDDFC,
                              .bar_tone = 0xC499F9,
                              .first_gradient_tone = 0xF7ECFD,
                              .second_gradient_tone = 0xD9BBFB,
                              .third_gradient_tone = 0xC499F9},
                    .dark = {.common_tone = 0xC499F9,
                             .background_tone = 0x562D9E,
                             .snapshot_background_tone = 0x280652,
                             .bar_tone = 0x562D9E,
                             .first_gradient_tone = 0x1C003C,
                             .second_gradient_tone = 0x40127F,
                             .third_gradient_tone = 0x562D9E}}},

        {tab_groups::TabGroupColorId::kCyan,
         ColorGroup{.light = {.common_tone = 0x28BAE6,
                              .background_tone = 0xD9F6FE,
                              .snapshot_background_tone = 0xAFECFD,
                              .bar_tone = 0x28BAE6,
                              .first_gradient_tone = 0xD9F6FE,
                              .second_gradient_tone = 0x6CD3F2,
                              .third_gradient_tone = 0x28BAE6},
                    .dark = {.common_tone = 0x28BAE6,
                             .background_tone = 0x0B4D66,
                             .snapshot_background_tone = 0x021F2C,
                             .bar_tone = 0x0B4D66,
                             .first_gradient_tone = 0x01141F,
                             .second_gradient_tone = 0x053547,
                             .third_gradient_tone = 0x0B4D66}}},

        {tab_groups::TabGroupColorId::kOrange,
         ColorGroup{.light = {.common_tone = 0xFC8F4F,
                              .background_tone = 0xFEEDE2,
                              .snapshot_background_tone = 0xFEDDC6,
                              .bar_tone = 0xFC8F4F,
                              .first_gradient_tone = 0xFEEDE2,
                              .second_gradient_tone = 0xFDB68B,
                              .third_gradient_tone = 0xFC8F4F},
                    .dark = {.common_tone = 0xFC8F4F,
                             .background_tone = 0x733610,
                             .snapshot_background_tone = 0x311303,
                             .bar_tone = 0x733610,
                             .first_gradient_tone = 0x210B00,
                             .second_gradient_tone = 0x512409,
                             .third_gradient_tone = 0x733610}}},
    });

// An updated map associating each `TabGroupColorId` with its ColorGroup tones,
// this one is used when SyncedGroupColor is enabled.
static constexpr auto kSyncedColorGroupMap =
    base::MakeFixedFlatMap<tab_groups::TabGroupColorId, ColorGroup>({

        {tab_groups::TabGroupColorId::kGrey,
         ColorGroup{.light = {.common_tone = 0xA6ABB4,
                              .background_tone = 0xEDF1FB,
                              .snapshot_background_tone = 0xDDE3EB,
                              .bar_tone = 0xA6ABB4,
                              .first_gradient_tone = 0xEDF1FB,
                              .second_gradient_tone = 0xC2C7D0,
                              .third_gradient_tone = 0xA6ABB4},
                    .dark = {.common_tone = 0xA6ABB4,
                             .background_tone = 0x424750,
                             .snapshot_background_tone = 0x161C24,
                             .bar_tone = 0x424750,
                             .first_gradient_tone = 0x0B1118,
                             .second_gradient_tone = 0x2B313A,
                             .third_gradient_tone = 0x424750}}},

        {tab_groups::TabGroupColorId::kBlue,
         ColorGroup{.light = {.common_tone = 0x87A9FE,
                              .background_tone = 0xEFF0FF,
                              .snapshot_background_tone = 0xDAE1FF,
                              .bar_tone = 0x87A9FE,
                              .first_gradient_tone = 0xEFF0FF,
                              .second_gradient_tone = 0xB1C6FF,
                              .third_gradient_tone = 0x87A9FE},
                    .dark = {.common_tone = 0x87A9FE,
                             .background_tone = 0x003DB4,
                             .snapshot_background_tone = 0x001650,
                             .bar_tone = 0x003DB4,
                             .first_gradient_tone = 0x000C38,
                             .second_gradient_tone = 0x002A80,
                             .third_gradient_tone = 0x003DB4}}},

        {tab_groups::TabGroupColorId::kRed,
         ColorGroup{.light = {.common_tone = 0xFF8177,
                              .background_tone = 0xFFECE9,
                              .snapshot_background_tone = 0xFFD9D5,
                              .bar_tone = 0xFF8177,
                              .first_gradient_tone = 0xFFECE9,
                              .second_gradient_tone = 0xFFB0A9,
                              .third_gradient_tone = 0xFF8177},
                    .dark = {.common_tone = 0xFF8177,
                             .background_tone = 0xA20000,
                             .snapshot_background_tone = 0x480000,
                             .bar_tone = 0xA20000,
                             .first_gradient_tone = 0x330000,
                             .second_gradient_tone = 0x730000,
                             .third_gradient_tone = 0xA20000}}},

        {tab_groups::TabGroupColorId::kGreen,
         ColorGroup{.light = {.common_tone = 0x3EC16D,
                              .background_tone = 0xB6FFC2,
                              .snapshot_background_tone = 0x7BFAA0,
                              .bar_tone = 0x3EC16D,
                              .first_gradient_tone = 0xB6FFC2,
                              .second_gradient_tone = 0x5EDD84,
                              .third_gradient_tone = 0x3EC16D},
                    .dark = {.common_tone = 0x3EC16D,
                             .background_tone = 0x005517,
                             .snapshot_background_tone = 0x002205,
                             .bar_tone = 0x005517,
                             .first_gradient_tone = 0x001602,
                             .second_gradient_tone = 0x003A0E,
                             .third_gradient_tone = 0x005517}}},

        {tab_groups::TabGroupColorId::kPink,
         ColorGroup{.light = {.common_tone = 0xFF73DF,
                              .background_tone = 0xFFEBF4,
                              .snapshot_background_tone = 0xFFD7EF,
                              .bar_tone = 0xFF73DF,
                              .first_gradient_tone = 0xFFEBF4,
                              .second_gradient_tone = 0xFFA9E5,
                              .third_gradient_tone = 0xFF73DF},
                    .dark = {.common_tone = 0xFF73DF,
                             .background_tone = 0x940074,
                             .snapshot_background_tone = 0x410032,
                             .bar_tone = 0x940074,
                             .first_gradient_tone = 0x2D0021,
                             .second_gradient_tone = 0x6A0052,
                             .third_gradient_tone = 0x940074}}},

        {tab_groups::TabGroupColorId::kPurple,
         ColorGroup{.light = {.common_tone = 0xC597FF,
                              .background_tone = 0xFAEEFF,
                              .snapshot_background_tone = 0xEFDCFF,
                              .bar_tone = 0xC597FF,
                              .first_gradient_tone = 0xFAEEFF,
                              .second_gradient_tone = 0xD9BBFF,
                              .third_gradient_tone = 0xC597FF},
                    .dark = {.common_tone = 0xC597FF,
                             .background_tone = 0x6900CA,
                             .snapshot_background_tone = 0x2C005C,
                             .bar_tone = 0x6900CA,
                             .first_gradient_tone = 0x1C0040,
                             .second_gradient_tone = 0x490091,
                             .third_gradient_tone = 0x6900CA}}},

        {tab_groups::TabGroupColorId::kCyan,
         ColorGroup{.light = {.common_tone = 0x30BCC3,
                              .background_tone = 0xBAFCFF,
                              .snapshot_background_tone = 0x77F4FC,
                              .bar_tone = 0x30BCC3,
                              .first_gradient_tone = 0xBAFCFF,
                              .second_gradient_tone = 0x57D7DF,
                              .third_gradient_tone = 0x30BCC3},
                    .dark = {.common_tone = 0x30BCC3,
                             .background_tone = 0x005154,
                             .snapshot_background_tone = 0x002124,
                             .bar_tone = 0x005154,
                             .first_gradient_tone = 0x001415,
                             .second_gradient_tone = 0x00373B,
                             .third_gradient_tone = 0x005154}}},

        {tab_groups::TabGroupColorId::kOrange,
         ColorGroup{.light = {.common_tone = 0xE59F00,
                              .background_tone = 0xFFEDD7,
                              .snapshot_background_tone = 0xFFDC9F,
                              .bar_tone = 0xE59F00,
                              .first_gradient_tone = 0xFFEDD7,
                              .second_gradient_tone = 0xFFBA01,
                              .third_gradient_tone = 0xE59F00},
                    .dark = {.common_tone = 0xE59F00,
                             .background_tone = 0x634100,
                             .snapshot_background_tone = 0x2A1800,
                             .bar_tone = 0x634100,
                             .first_gradient_tone = 0x1A0F00,
                             .second_gradient_tone = 0x452C00,
                             .third_gradient_tone = 0x634100}}},

        {tab_groups::TabGroupColorId::kYellow,
         ColorGroup{.light = {.common_tone = 0x9BB62F,
                              .background_tone = 0xF3FFBC,
                              .snapshot_background_tone = 0xD1EF65,
                              .bar_tone = 0x9BB62F,
                              .first_gradient_tone = 0xF3FFBC,
                              .second_gradient_tone = 0xB6D249,
                              .third_gradient_tone = 0x9BB62F},
                    .dark = {.common_tone = 0x9BB62F,
                             .background_tone = 0x3C4C00,
                             .snapshot_background_tone = 0x161E00,
                             .bar_tone = 0x3C4C00,
                             .first_gradient_tone = 0x0D1300,
                             .second_gradient_tone = 0x293400,
                             .third_gradient_tone = 0x3C4C00}}}});

static_assert(kColorGroupMap.size() ==
                  static_cast<size_t>(TabGroupColorId::kNumEntries),
              "");

static_assert(kSyncedColorGroupMap.size() ==
                  static_cast<size_t>(TabGroupColorId::kNumEntries),
              "");

// Returns the `ColorGroup` corresponding to the tabGroupColorId.
const ColorGroup& GetColorGroupForId(TabGroupColorId tab_group_color_id) {
  if (IsSyncedGroupColorEnabled()) {
    return kSyncedColorGroupMap.at(tab_group_color_id);
  }

  return kColorGroupMap.at(tab_group_color_id);
}

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
  const ColorGroup& group = GetColorGroupForId(tabGroupColorID);

  return CreateDynamicProviderFromRGB(group.light.common_tone,
                                      group.dark.common_tone);
}

+ (NSArray<UIColor*>*)gradientBackgroundColors:
    (tab_groups::TabGroupColorId)tabGroupColorID {
  const ColorGroup& group = GetColorGroupForId(tabGroupColorID);

  return @[
    CreateDynamicProviderFromRGB(group.light.first_gradient_tone,
                                 group.dark.first_gradient_tone),
    CreateDynamicProviderFromRGB(group.light.second_gradient_tone,
                                 group.dark.second_gradient_tone),
    CreateDynamicProviderFromRGB(group.light.third_gradient_tone,
                                 group.dark.third_gradient_tone),
  ];
}

- (instancetype)initWithColorId:(TabGroupColorId)tabGroupColorID {
  self = [super init];
  if (self) {
    _tabGroupColorID = tabGroupColorID;
    const ColorGroup& group = GetColorGroupForId(tabGroupColorID);

    _backgroundColor = CreateDynamicProviderFromRGB(group.light.background_tone,
                                                    group.dark.background_tone);
    _snapshotBackgroundColor =
        CreateDynamicProviderFromRGB(group.light.snapshot_background_tone,
                                     group.dark.snapshot_background_tone);
    _barColor =
        CreateDynamicProviderFromRGB(group.light.bar_tone, group.dark.bar_tone,
                                     kLightBarToneAlpha, kDarkBarToneAlpha);
    _commonColor = CreateDynamicProviderFromRGB(group.light.common_tone,
                                                group.dark.common_tone);
  }

  return self;
}

@end
