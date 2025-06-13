// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_color_picker_mediator.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_color_palette_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "skia/ext/skia_utils_ios.h"

namespace {
// Array of seed colors (in RGB integer format) used to generate background
// color palette configurations in the color picker.
const SkColor kSeedColorRGBs[] = {
    0x8cabe4,  // Blue.
    0x26a69a,  // Aqua.
    0x00ff00,  // Green.
    0x87ba81,  // Viridian.
    0xfadf73,  // Citron.
    0xff8000,  // Orange.
    0xf3b2be,  // Rose.
    0xff00ff,  // Fuchsia.
    0xe5d5fc   // Violet.
};

// Returns a dynamic UIColor using two named color assets for light and dark
// mode.
UIColor* DynamicNamedColor(NSString* lightName, NSString* darkName) {
  return
      [UIColor colorWithDynamicProvider:^UIColor*(UITraitCollection* traits) {
        BOOL isDark = (traits.userInterfaceStyle == UIUserInterfaceStyleDark);
        return [UIColor colorNamed:isDark ? darkName : lightName];
      }];
}

}  // namespace

@implementation HomeCustomizationBackgroundColorPickerMediator

- (void)configureColorPalettes {
  NSMutableArray* configs = [NSMutableArray array];

  HomeCustomizationColorPaletteConfiguration* defaultColorPalette =
      [[HomeCustomizationColorPaletteConfiguration alloc] init];

  // The first choice should be the "no background" option (default appearance
  // colors).
  defaultColorPalette.lightColor =
      DynamicNamedColor(@"ntp_background_color", kTertiaryBackgroundColor);
  defaultColorPalette.mediumColor =
      [UIColor colorNamed:@"fake_omnibox_solid_background_color"];
  defaultColorPalette.darkColor =
      DynamicNamedColor(kBlueColor, kTextPrimaryColor);

  [configs addObject:defaultColorPalette];

  for (int rgb : kSeedColorRGBs) {
    [configs addObject:CreateColorPaletteConfigurationFromSeedColor(
                           UIColorFromRGB(rgb))];
  }

  // TODO(crbug.com/408243803): Pass the current selection ID if the background
  // is a color; pass 0 if the background is set to "no background".
  [_consumer setColorPaletteConfigurations:configs selectedColorIndex:@(0)];
}

@end
