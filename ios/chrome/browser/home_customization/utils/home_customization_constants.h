// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

// Represents the section identifiers of the customization menu as an NSString.
typedef NSString CustomizationSection;

// The section identifier for the main menu's visibility toggles.
extern CustomizationSection* const kCustomizationSectionToggles;

// The identifier for the bottom sheet's initial detent.
extern NSString* const kBottomSheetDetentIdentifier;

// The identifier for the dismiss button on the top right of the navigation bar.
extern NSString* const kNavigationBarDismissButtonIdentifier;

// Enum representing the customization submenus that can be navigated to.
enum class CustomizationMenuPage : NSInteger {
  kCustomizationMenuPageMain,
  kCustomizationMenuPageMagicStack,
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_
