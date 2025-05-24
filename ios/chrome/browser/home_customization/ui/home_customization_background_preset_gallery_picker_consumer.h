// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/home_customization/model/background_collection_configuration.h"

// A consumer protocol that receives updates about background galleries.
@protocol HomeCustomizationBackgroundPresetGalleryPickerConsumer

// Set the background collection configurations, including section data and
// the selected background identifier. This method also sets the data source
// with the appropriate configuration options for each section.
- (void)setBackgroundCollectionConfigurations:
            (NSArray<BackgroundCollectionConfiguration*>*)
                backgroundCollectionConfigurations
                         selectedBackgroundId:(NSString*)selectedBackgroundId;
@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_CONSUMER_H_
