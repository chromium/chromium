// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_color_picker_mediator.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_color_palette_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
UIColor* const kBlue = UIColorFromRGB(0x8cabe4);
UIColor* const kAqua = UIColorFromRGB(0x26a69a);
UIColor* const kGreen = UIColorFromRGB(0x00ff00);
UIColor* const kViridian = UIColorFromRGB(0x87ba81);
UIColor* const kCitron = UIColorFromRGB(0xfadf73);
UIColor* const kOrange = UIColorFromRGB(0xff8000);
UIColor* const kRose = UIColorFromRGB(0xf3b2be);
UIColor* const kFuchsia = UIColorFromRGB(0xff00ff);
UIColor* const kViolet = UIColorFromRGB(0xe5d5fc);
}  // namespace

@implementation HomeCustomizationBackgroundColorPickerMediator

- (void)configureColorPalettes {
  NSMutableArray* configs = [NSMutableArray array];

  HomeCustomizationColorPaletteConfiguration* defaultColorPalette =
      [[HomeCustomizationColorPaletteConfiguration alloc] init];

  // The first choice should be the "no background" option (default appearance
  // colors).
  defaultColorPalette.lightColor = [UIColor colorNamed:@"ntp_background_color"];
  defaultColorPalette.darkColor = [UIColor colorNamed:kBlue600Color];
  defaultColorPalette.mediumColor =
      [UIColor colorNamed:@"fake_omnibox_solid_background_color"];
  [configs addObject:defaultColorPalette];

  [@[
    kBlue, kAqua, kGreen, kViridian, kCitron, kOrange, kRose, kFuchsia, kViolet
  ] enumerateObjectsUsingBlock:^(UIColor* seedColor, NSUInteger index,
                                 BOOL* stop) {
    [configs addObject:CreateColorPaletteConfigurationFromSeedColor(seedColor)];
  }];

  [_consumer setColorPaletteConfigurations:configs selectedColorIndex:nil];
}

@end
