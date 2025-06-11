// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_color_picker_mediator.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_color_palette_util.h"

@implementation HomeCustomizationBackgroundColorPickerMediator

- (void)configureColorPalettes {
  NSMutableArray* configs = [NSMutableArray array];

  // TODO(crbug.com/408243803):Add the UIColor seeds that will be used by Monet
  // to generate the color palettes.
  [@[
    [UIColor redColor], [UIColor blueColor], [UIColor greenColor],
    [UIColor orangeColor], [UIColor purpleColor]
  ] enumerateObjectsUsingBlock:^(UIColor* seedColor, NSUInteger index,
                                 BOOL* stop) {
    [configs addObject:CreateColorPaletteConfigurationFromSeedColor(seedColor)];
  }];

  [_consumer setColorPaletteConfigurations:configs selectedColorIndex:nil];
}

@end
