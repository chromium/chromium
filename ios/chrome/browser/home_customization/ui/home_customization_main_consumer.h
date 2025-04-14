// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MAIN_CONSUMER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MAIN_CONSUMER_H_

#import <UIKit/UIKit.h>

#include <map>

#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

// Consumer protocol for the HomeCustomizationMediator to provide data to the
// main page's view controller.
@protocol HomeCustomizationMainConsumer

// Populates the toggle cells with a map of types and bools indicating if each
// type is enabled, then updates the snapshot.
- (void)populateToggles:(std::map<CustomizationToggleType, BOOL>)toggleMap;

// Populates the background customization cells with a map of options, then
// update the snapshot.
- (void)
    populateBackgroundCustomizationConfigurations:
        (NSMutableDictionary<NSString*, BackgroundCustomizationConfiguration*>*)
            BackgroundCustomizationConfigurationMap
                             selectedBackgroundId:
                                 (NSString*)selectedBackgroundId;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MAIN_CONSUMER_H_
