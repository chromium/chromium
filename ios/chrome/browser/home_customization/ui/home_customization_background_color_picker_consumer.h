// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_color_palette_configuration.h"

// A consumer protocol that receives updates about background color palettes.
@protocol HomeCustomizationBackgroundColorPickerConsumer

// Sets the background color palette configurations for each seed color,
// and specifies the index of the currently selected background color.
- (void)setColorPaletteConfigurations:
            (NSArray<HomeCustomizationColorPaletteConfiguration*>*)
                colorPaletteConfigurations
                   selectedColorIndex:(NSNumber*)selectedColorIndex;
@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_CONSUMER_H_
