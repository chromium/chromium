// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"

#import "base/notreached.h"

NSString* const kContentSuggestionsCollectionIdentifier =
    @"ContentSuggestionsCollectionIdentifier";

NSString* const kContentSuggestionsLearnMoreIdentifier = @"Learn more";

NSString* const kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix =
    @"contentSuggestionsMostVisitedAccessibilityIdentifierPrefix";

NSString* const kContentSuggestionsShortcutsAccessibilityIdentifierPrefix =
    @"contentSuggestionsShortcutsAccessibilityIdentifierPrefix";

NSString* const kMagicStackScrollViewAccessibilityIdentifier =
    @"MagicStackScrollViewAccessibilityIdentifier";

NSString* const kMagicStackEditButtonContainerAccessibilityIdentifier =
    @"MagicStackEditButtonContainerAccessibilityIdentifier";

NSString* const kMagicStackEditButtonAccessibilityIdentifier =
    @"MagicStackEditButtonAccessibilityIdentifier";

NSString* const kMagicStackEditHalfSheetDoneButtonAccessibilityIdentifier =
    @"MagicStackEditHalfSheetDoneButtonAccessibilityIdentifier";

NSString* const kMagicStackViewAccessibilityIdentifier = @"kMagicStack";

const CGFloat kMagicStackWideWidth = 430;

const CGFloat kMostVisitedBottomMargin = 13;

const CGFloat kMagicStackFaviconWidth = 28;


ContentSuggestionsModuleType SetUpListModuleTypeForSetUpListType(
    SetUpListItemType type) {
  switch (type) {
    case SetUpListItemType::kSignInSync:
      return ContentSuggestionsModuleType::kSetUpListSync;
    case SetUpListItemType::kDefaultBrowser:
      return ContentSuggestionsModuleType::kSetUpListDefaultBrowser;
    case SetUpListItemType::kAutofill:
      return ContentSuggestionsModuleType::kSetUpListAutofill;
    case SetUpListItemType::kAllSet:
      return ContentSuggestionsModuleType::kSetUpListAllSet;
    default:
      NOTREACHED_NORETURN();
  }
}
