// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"

#import "base/notreached.h"
#import "components/ntp_tiles/features.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"

namespace {

// The maximum number of Most Visited tiles shown.
constexpr NSUInteger kContentSuggestionsMostVisitedTilesMax = 8;

// The maximum number of Most Visited tiles shown when the AIM tile is eligible.
constexpr NSUInteger kContentSuggestionsMostVisitedTilesMaxWithAim =
    kContentSuggestionsMostVisitedTilesMax + 1;

}  // namespace

NSString* const kContentSuggestionsCollectionIdentifier =
    @"ContentSuggestionsCollectionIdentifier";

NSString* const kContentSuggestionsLearnMoreIdentifier = @"Learn more";

NSString* const kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix =
    @"contentSuggestionsMostVisitedAccessibilityIdentifierPrefix";

NSString* const kContentSuggestionsShortcutsAccessibilityIdentifierPrefix =
    @"contentSuggestionsShortcutsAccessibilityIdentifierPrefix";

NSString* const kMagicStackScrollViewAccessibilityIdentifier =
    @"MagicStackScrollViewAccessibilityIdentifier";

NSString* const kMagicStackEditHalfSheetDoneButtonAccessibilityIdentifier =
    @"MagicStackEditHalfSheetDoneButtonAccessibilityIdentifier";

NSString* const kMagicStackViewAccessibilityIdentifier = @"kMagicStack";

NSString* const
    kMagicStackContentSuggestionsModuleTabResumptionAccessibilityIdentifier =
        @"MagicStackContentSuggestionsModuleTabResumption"
        @"AccessibilityIdentifier";

const CGFloat kMagicStackWideWidth = 430;

const CGFloat kMostVisitedBottomMargin = 13;

const CGFloat kMagicStackFaviconWidth = 28;

ContentSuggestionsModuleType SetUpListModuleTypeForSetUpListType(
    SetUpListItemType type) {
  switch (type) {
    case SetUpListItemType::kDefaultBrowser:
      return ContentSuggestionsModuleType::kSetUpListDefaultBrowser;
    case SetUpListItemType::kAutofill:
      return ContentSuggestionsModuleType::kSetUpListAutofill;
    case SetUpListItemType::kAllSet:
      return ContentSuggestionsModuleType::kSetUpListAllSet;
    case SetUpListItemType::kNotifications:
      return ContentSuggestionsModuleType::kSetUpListNotifications;
    default:
      NOTREACHED();
  }
}

bool IsTipsModuleType(ContentSuggestionsModuleType type) {
  switch (type) {
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kSetUpListAllSet:
    case ContentSuggestionsModuleType::kTipsWithProductImage:
    case ContentSuggestionsModuleType::kTips:
    case ContentSuggestionsModuleType::kAppBundlePromo:
    case ContentSuggestionsModuleType::kDefaultBrowser:
      return true;
    case ContentSuggestionsModuleType::kInvalid:
    case ContentSuggestionsModuleType::kTabResumption:
    case ContentSuggestionsModuleType::kSafetyCheck:
    case ContentSuggestionsModuleType::kMostVisited:
    case ContentSuggestionsModuleType::kShortcuts:
    case ContentSuggestionsModuleType::kPlaceholder:
    case ContentSuggestionsModuleType::kPriceTrackingPromo:
    case ContentSuggestionsModuleType::kSendTabPromo:
    case ContentSuggestionsModuleType::kShopCard:
    case ContentSuggestionsModuleType::kLevelUp:
      return false;
  }
}

NSUInteger MaximumMostVisitedTilesCount() {
  if (IsAimEnabledInNtp() && ntp_tiles::GetAimButtonRefactorArm() ==
                                 ntp_tiles::AimButtonRefactorArm::kAimAsMvt) {
    return kContentSuggestionsMostVisitedTilesMaxWithAim;
  }
  return kContentSuggestionsMostVisitedTilesMax;
}
