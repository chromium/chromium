// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_HELPER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_HELPER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

// Helper for Home customization elements.
@interface HomeCustomizationHelper : NSObject

// Returns the title text for a given toggle type.
+ (NSString*)titleForToggleType:(CustomizationToggleType)type;

// Returns the subtitle text for a given toggle type.
+ (NSString*)subtitleForToggleType:(CustomizationToggleType)type;

// Returns the icon image for a given toggle type.
+ (UIImage*)iconForToggleType:(CustomizationToggleType)type;

// Returns the accessibility identifier for a given toggle type.
+ (NSString*)accessibilityIdentifierForToggleType:(CustomizationToggleType)type;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_HELPER_H_
