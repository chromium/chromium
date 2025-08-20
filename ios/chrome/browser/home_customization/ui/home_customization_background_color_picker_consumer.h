// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_CONSUMER_H_

#import <Foundation/Foundation.h>

@protocol BackgroundCustomizationConfiguration;

// A consumer protocol that receives updates about background color palettes.
@protocol HomeCustomizationBackgroundColorPickerConsumer

// Sets the background configuration for each color,
// and specifies the index of the currently selected background configuration.
- (void)populateBackgroundCustomizationConfigurations:
            (NSArray<id<BackgroundCustomizationConfiguration>>*)
                backgroundCustomizationConfigurations
                                   selectedColorIndex:
                                       (NSNumber*)selectedColorIndex;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_CONSUMER_H_
