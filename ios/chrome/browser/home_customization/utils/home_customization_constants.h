// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Represents the section identifiers of the customization menu as an NSString.
typedef NSString CustomizationSection;

// The section identifier for the main menu's visibility toggles.
extern CustomizationSection* const kCustomizationSectionToggles;

// The identifier for the bottom sheet's initial detent.
extern NSString* const kBottomSheetDetentIdentifier;

// The identifier for the dismiss button on the top right of the navigation bar.
extern NSString* const kNavigationBarDismissButtonIdentifier;

// The identifiers for the main page toggle cells.
extern NSString* const kCustomizationToggleMostVisitedIdentifier;
extern NSString* const kCustomizationToggleMagicStackIdentifier;
extern NSString* const kCustomizationToggleDiscoverIdentifier;

// The size of the toggle cell's icon.
extern const CGFloat kToggleIconPointSize;

// Enum representing the customization submenus that can be navigated to.
enum class CustomizationMenuPage : NSInteger {
  kMain,
  kMagicStack,
};

// Enum representing the toggle cells to control module visibility.
enum class CustomizationToggleType : NSInteger {
  kMostVisited,
  kMagicStack,
  kDiscover,
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_
