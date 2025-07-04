// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"

// A consumer protocol that receives updates about background color palettes.
@protocol HomeCustomizationBackgroundColorPickerConsumer

// Sets the background color palette for each seed color,
// and specifies the index of the currently selected background color.
- (void)setColorPalettes:(NSArray<NewTabPageColorPalette*>*)colorPalettes
      selectedColorIndex:(NSNumber*)selectedColorIndex;
@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_CONSUMER_H_
