// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"

// Enum specifying the type of Content Suggestions a module is showing.
// Entries should always keep synced with the IOSMagicStackModuleType histogram
// enum. Entries should not be renumbered and numeric values should never be
// reused.
// LINT.IfChange
enum class ContentSuggestionsModuleType {
  kInvalid = -1,
  kMostVisited = 0,
  kShortcuts = 1,
  kSetUpListSync = 2,
  kSetUpListDefaultBrowser = 3,
  kSetUpListAutofill = 4,
  kCompactedSetUpList = 5,
  kSetUpListAllSet = 6,
  kSafetyCheck = 7,
  // Removed: kSafetyCheckMultiRow = 8,
  // Removed: kSafetyCheckMultiRowOverflow = 9,
  kTabResumption = 10,
  kParcelTracking = 11,
  // Removed: kParcelTrackingSeeMore = 12,
  kSetUpListNotifications = 13,
  kPlaceholder = 14,
  kPriceTrackingPromo = 15,
  // Larger variant of `kTips` with different layout/formatting for displaying
  // larger-sized product images within the module.
  //
  // TODO(crbug.com/370479820): Deprecate when Magic Stack supports dynamic
  // styling and layout decoupled from `ContentSuggestionsModuleType`.
  kTipsWithProductImage = 16,
  kTips = 17,
  kMaxValue = kTips,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Enum for content notification promo events UMA metrics. Entries should not
// be renumbered and numeric values should never be reused. This should align
// with the ContentNotificationSnackbarEvent enum in enums.xml.
//
// LINT.IfChange
enum class ContentNotificationSnackbarEvent {
  kShown = 0,
  kActionButtonTapped = 1,
  kMaxValue = kActionButtonTapped,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/content/enums.xml)

// Represents the content suggestions collection view.
extern NSString* const kContentSuggestionsCollectionIdentifier;

// Represents the Learn More button in the content suggestions.
extern NSString* const kContentSuggestionsLearnMoreIdentifier;

// Represents the most visited tiles of the content suggestions.
extern NSString* const
    kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix;

// Represents the shortcuts of the content suggestions.
extern NSString* const
    kContentSuggestionsShortcutsAccessibilityIdentifierPrefix;

// Represents the Magic Stack ScrollView.
extern NSString* const kMagicStackScrollViewAccessibilityIdentifier;

// Represents the Magic Stack UIStackView.
extern NSString* const kMagicStackViewAccessibilityIdentifier;

// Represents the "Done" button in the Magic Stack edit half sheet.
extern NSString* const
    kMagicStackEditHalfSheetDoneButtonAccessibilityIdentifier;

// Represents the "Continue with This Tab" module in the magic stack.
extern NSString* const
    kMagicStackContentSuggestionsModuleTabResumptionAccessibilityIdentifier;

// Represents the width of the Magic Stack ScrollView for the unique wide
// layout.
extern const CGFloat kMagicStackWideWidth;

// The bottom margin below the Most Visited section.
extern const CGFloat kMostVisitedBottomMargin;

// Most Visited Tiles favicon width when kMagicStack is enabled.
extern const CGFloat kMagicStackFaviconWidth;

// Returns the matching ContentSuggestionsModuleType for a given
// SetUpListItemType `type`.
ContentSuggestionsModuleType SetUpListModuleTypeForSetUpListType(
    SetUpListItemType type);

// Returns true if the module type is one of the SetUpList types.
bool IsSetUpListModuleType(ContentSuggestionsModuleType type);

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_
