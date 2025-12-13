// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Represents the section identifiers of the customization menu as an NSString.
typedef NSString CustomizationSection;

// The section identifier for the main menu's background customizations.
extern CustomizationSection* const kCustomizationSectionBackground;

// The section identifier for the enterprise message.
extern CustomizationSection* const kCustomizationSectionEnterprise;

// The section identifier for the main menu's visibility toggles.
extern CustomizationSection* const kCustomizationSectionMainToggles;

// The section identifier for the Discover submenu's links.
extern CustomizationSection* const kCustomizationSectionDiscoverLinks;

// The section identifier for the Magic Stack menu's visibility toggles.
extern CustomizationSection* const kCustomizationSectionMagicStackToggles;

// The identifier for the bottom sheet's initial detent.
extern NSString* const kBottomSheetDetentIdentifier;

// The identifier for the bottom sheet's expanded height detent (if one exists
// for the current page).
extern NSString* const kBottomSheetExpandedDetentIdentifier;

// The identifier for the dismiss button on the top right of the navigation bar.
extern NSString* const kNavigationBarDismissButtonIdentifier;

// The identifier for the back button on the top left of the navigation bar.
extern NSString* const kNavigationBarBackButtonIdentifier;

// The identifiers for the main page toggle cells.
extern NSString* const kCustomizationToggleMostVisitedIdentifier;
extern NSString* const kCustomizationToggleMagicStackIdentifier;
extern NSString* const kCustomizationToggleDiscoverIdentifier;

// The identifiers for the Magic Stack page toggle cells.
extern NSString* const kCustomizationToggleSafetyCheckIdentifier;
extern NSString* const kCustomizationToggleTabResumptionIdentifier;
extern NSString* const kCustomizationToggleShopCardPriceTrackingIdentifier;
extern NSString* const kCustomizationToggleShopCardReviewsIdentifier;
extern NSString* const kCustomizationToggleTipsIdentifier;

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

// The identifiers for the background cells.
extern NSString* const kBackgroundCellIdentifier;

// The identifier for the background picker cell.
extern NSString* const kBackgroundPickerCellIdentifier;

// The identifier for the enterprise message cell.
extern NSString* const kEnterpriseCellIdentifier;

// The URLs for the links in the Discover submenu.
extern const char kDiscoverFollowingURL[];
extern const char kDiscoverHiddenURL[];
extern const char kDiscoverActivityURL[];
extern const char kDiscoverLearnMoreURL[];

// The number of recent backgrounds to show.
extern const NSInteger kNumberOfRecentBackgrounds;

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
  kTips,
  kSafetyCheck,
  kTapResumption,
  kShopCard,
};

// Enum representing the link cells to navigate to external URLs.
enum class CustomizationLinkType : NSInteger {
  kFollowing,
  kHidden,
  kActivity,
  kLearnMore,
  kEnterpriseLearnMore,
};

// Represents the background style used for home customization.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(HomeCustomizationBackgroundStyle)
enum class HomeCustomizationBackgroundStyle : NSInteger {
  // No background (default appearance).
  kDefault,

  // Solid background color.
  kColor,

  // Background chosen from preset gallery.
  kPreset,

  // User-uploaded background image.
  kUserUploaded,

  // Must be last.
  kMaxValue = kUserUploaded,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSHomeCustomizationBackgroundStyle)

// Records the final outcome of the user's background selection flow.
// This is logged once the user either applies a background or exits
// the customization UI.
// LINT.IfChange(BackgroundSelectionOutcome)
enum class BackgroundSelectionOutcome : NSInteger {
  // User selected and applied a new background.
  kApplied,

  // User opened the customization UI but closed it without
  // ever selecting a background.
  kCanceled,

  // User selected a background, but then canceled instead of applying it.
  kCanceledAfterSelected,

  // Must be last.
  kMaxValue = kCanceledAfterSelected,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSBackgroundSelectionOutcome)

// Records errors that can occur during the user uploaded image.
// LINT.IfChange(UserUploadedImageError)
enum class UserUploadedImageError : NSInteger {
  // No error.
  kNone,

  // Failed to create the directory for storing the image.
  kFailedToCreateDirectory,

  // Failed to write the image data to a file.
  kFailedToWriteFile,

  // Failed to convert the image to JPEG.
  kFailedToConvertToJPEG,

  // Failed to read the image data from its file path.
  kFailedToReadFile,

  // Failed to create a UIImage from the loaded data.
  kFailedToCreateImageFromData,

  // Must be last.
  kMaxValue = kFailedToCreateImageFromData,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSUserUploadedImageError)

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_
