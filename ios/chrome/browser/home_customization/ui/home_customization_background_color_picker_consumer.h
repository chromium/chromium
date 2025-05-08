// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_color_palette_configuration.h"

// A consumer protocol that receives updates about background color palettes.
@protocol HomeCustomizationBackgroundColorPickerConsumer

// Sets the available background color palette configurations, indexed by seed
// color index.
- (void)setColorPaletteConfigurations:
    (NSArray<HomeCustomizationColorPaletteConfiguration*>*)
        colorPaletteConfigurations;
@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_CONSUMER_H_
