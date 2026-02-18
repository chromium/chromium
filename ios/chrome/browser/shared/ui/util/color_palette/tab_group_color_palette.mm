// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/color_palette/tab_group_color_palette.h"

#import "base/notreached.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/ui/util/color_palette/color_palette_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

namespace {

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

- (instancetype)initWithColorId:
    (tab_groups::TabGroupColorId)tab_group_color_id {
  self = [super init];
  if (self) {
    int tone10;
    int tone30;
    int tone70;
    int tone90;
    int tone95;
    switch (tab_group_color_id) {
      case tab_groups::TabGroupColorId::kGrey: {
        tone10 = 0x191D1C;
        tone30 = 0x444746;
        tone70 = 0xA9ACAA;
        tone90 = 0xE3E3E3;
        tone95 = 0xF2F2F2;

        break;
      }
      case tab_groups::TabGroupColorId::kBlue: {
        tone10 = 0x021942;
        tone30 = 0x0F419A;
        tone70 = 0x7AACF9;
        tone90 = 0xD1E4FD;
        tone95 = 0xE7F2FE;

        break;
      }
      case tab_groups::TabGroupColorId::kRed: {
        tone10 = 0x390B09;
        tone30 = 0x88201B;
        tone70 = 0xFC8C86;
        tone90 = 0xFEDBDC;
        tone95 = 0xFEECEE;

        break;
      }
      case tab_groups::TabGroupColorId::kYellow: {
        tone10 = 0x2E1503;
        tone30 = 0x6B3B10;
        tone70 = 0xEC9932;
        tone90 = 0xFEE089;
        tone95 = 0xFEF2BA;

        break;
      }
      case tab_groups::TabGroupColorId::kGreen: {
        tone10 = 0x022111;
        tone30 = 0x0A5130;
        tone70 = 0x4DC06F;
        tone90 = 0xC0EEBF;
        tone95 = 0xDEF7DB;

        break;
      }
      case tab_groups::TabGroupColorId::kPink: {
        tone10 = 0x3C0322;
        tone30 = 0x8A1051;
        tone70 = 0xFC82CE;
        tone90 = 0xFED9EE;
        tone95 = 0xFEECF5;

        break;
      }
      case tab_groups::TabGroupColorId::kPurple: {
        tone10 = 0x280652;
        tone30 = 0x562D9E;
        tone70 = 0xC499F9;
        tone90 = 0xEDDDFC;
        tone95 = 0xF7ECFD;

        break;
      }
      case tab_groups::TabGroupColorId::kCyan: {
        tone10 = 0x021F2C;
        tone30 = 0x0B4D66;
        tone70 = 0x28BAE6;
        tone90 = 0xAFECFD;
        tone95 = 0xD9F6FE;

        break;
      }
      case tab_groups::TabGroupColorId::kOrange: {
        tone10 = 0x311303;
        tone30 = 0x733610;
        tone70 = 0xFC8F4F;
        tone90 = 0xFEDDC6;
        tone95 = 0xFEEDE2;

        break;
      }
      default:
        NOTREACHED();
    }
    _backgroundColor = CreateDynamicProviderFromRGB(tone95, tone30);
    _snapshotBackgroundColor = CreateDynamicProviderFromRGB(tone90, tone10);
    _barColor = CreateDynamicProviderFromRGB(tone70, tone30, kLightBarToneAlpha,
                                             kDarkBarToneAlpha);
    _commonColor = CreateDynamicProviderFromRGB(tone70, tone70);
  }

  return self;
}

@end
