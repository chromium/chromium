// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Represents the section identifiers of the customization menu as an NSString.
typedef NSString CustomizationSection;

// The section identifier for the main menu's visibility toggles.
extern CustomizationSection* const kCustomizationSectionMainToggles;

// The section identifier for the Discover submenu's links.
extern CustomizationSection* const kCustomizationSectionDiscoverLinks;

// The section identifier for the Magic Stack menu's visibility toggles.
extern CustomizationSection* const kCustomizationSectionMagicStackToggles;

// The identifier for the bottom sheet's initial detent.
extern NSString* const kBottomSheetDetentIdentifier;

// The identifier for the dismiss button on the top right of the navigation bar.
extern NSString* const kNavigationBarDismissButtonIdentifier;

// The identifier for the back button on the top left of the navigation bar.
extern NSString* const kNavigationBarBackButtonIdentifier;

// The identifiers for the main page toggle cells.
extern NSString* const kCustomizationToggleMostVisitedIdentifier;
extern NSString* const kCustomizationToggleMagicStackIdentifier;
extern NSString* const kCustomizationToggleDiscoverIdentifier;

// The identifiers for the Magic Stack page toggle cells.
extern NSString* const kCustomizationToggleSetUpListIdentifier;
extern NSString* const kCustomizationToggleSafetyCheckIdentifier;
extern NSString* const kCustomizationToggleTabResumptionIdentifier;
extern NSString* const kCustomizationToggleParcelTrackingIdentifier;

// The identifiers for the navigable portions of the main page toggle cells.
extern NSString* const kCustomizationToggleMostVisitedNavigableIdentifier;
extern NSString* const kCustomizationToggleMagicStackNavigableIdentifier;
extern NSString* const kCustomizationToggleDiscoverNavigableIdentifier;

// The identifiers for the Discover page's link cells.
extern NSString* const kCustomizationLinkFollowingIdentifier;
extern NSString* const kCustomizationLinkHiddenIdentifier;
extern NSString* const kCustomizationLinkActivityIdentifier;
extern NSString* const kCustomizationLinkLearnMoreIdentifier;

// The identifiers for each menu page's collection view.
extern NSString* const kCustomizationCollectionMainIdentifier;
extern NSString* const kCustomizationCollectionMagicStackIdentifier;
extern NSString* const kCustomizationCollectionDiscoverIdentifier;

// The URLs for the links in the Discover submenu.
extern const char kDiscoverFollowingURL[];
extern const char kDiscoverHiddenURL[];
extern const char kDiscoverActivityURL[];
extern const char kDiscoverLearnMoreURL[];

// The size of the toggle cell's icon.
extern const CGFloat kToggleIconPointSize;

// Enum representing the customization submenus that can be navigated to.
enum class CustomizationMenuPage : NSInteger {
  kMain,
  kMagicStack,
  kDiscover,
  kUnknown,
};

// Enum representing the toggle cells to control module visibility.
enum class CustomizationToggleType : NSInteger {
  // Main page toggles.
  kMostVisited,
  kMagicStack,
  kDiscover,

  // Magic Stack page toggles.
  kSetUpList,
  kSafetyCheck,
  kTapResumption,
  kParcelTracking,
};

// Enum representing the link cells to navigate to external URLs.
enum class CustomizationLinkType : NSInteger {
  kFollowing,
  kHidden,
  kActivity,
  kLearnMore,
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_
