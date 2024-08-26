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

// Returns the accessibility identifier for the navigable portion of a given
// toggle type.
+ (NSString*)navigableAccessibilityIdentifierForToggleType:
    (CustomizationToggleType)type;

// Returns the menu page identifier for a given toggle type.
+ (CustomizationMenuPage)menuPageForToggleType:(CustomizationToggleType)type;

// Returns the title text for a given link type.
+ (NSString*)titleForLinkType:(CustomizationLinkType)type;

// Returns the subtitle text for a given link type.
+ (NSString*)subtitleForLinkType:(CustomizationLinkType)type;

// Returns the accessibility identifier for a given link type.
+ (NSString*)accessibilityIdentifierForLinkType:(CustomizationLinkType)type;

// Returns the text of the header for a given menu page.
+ (NSString*)headerTextForPage:(CustomizationMenuPage)page;

// Returns the navigation bar's title for a given menu page.
+ (NSString*)navigationBarTitleForPage:(CustomizationMenuPage)page;

// Returns the accessibility identifier for the collection view of a menu page.
+ (NSString*)accessibilityIdentifierForPageCollection:
    (CustomizationMenuPage)page;

// Returns `YES` if the given type supports navigation to a submenu.
+ (BOOL)doesTypeHaveSubmenu:(CustomizationToggleType)type;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_HELPER_H_
