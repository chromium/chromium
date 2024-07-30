// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation HomeCustomizationHelper

+ (NSString*)titleForToggleType:(CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kMostVisited:
      return @"Test title 1 (Most visited)";
    case CustomizationToggleType::kMagicStack:
      return @"Test title 2 (Magic Stack)";
    case CustomizationToggleType::kDiscover:
      return @"Test title 3 (Discover)";
  }
}

+ (NSString*)subtitleForToggleType:(CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kMostVisited:
      return @"Test subtitle 1";
    case CustomizationToggleType::kMagicStack:
      return @"Test subtitle 2";
    case CustomizationToggleType::kDiscover:
      return @"Test subtitle 3";
  }
}

+ (UIImage*)iconForToggleType:(CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kMostVisited:
      return DefaultSymbolWithPointSize(kHistorySymbol, kToggleIconPointSize);
    case CustomizationToggleType::kMagicStack:
      return DefaultSymbolWithPointSize(kMagicStackSymbol,
                                        kToggleIconPointSize);
    case CustomizationToggleType::kDiscover:
      return DefaultSymbolWithPointSize(kDiscoverFeedSymbol,
                                        kToggleIconPointSize);
  }
}

+ (NSString*)accessibilityIdentifierForToggleType:
    (CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kMostVisited:
      return kCustomizationToggleMostVisitedIdentifier;
    case CustomizationToggleType::kMagicStack:
      return kCustomizationToggleMagicStackIdentifier;
    case CustomizationToggleType::kDiscover:
      return kCustomizationToggleDiscoverIdentifier;
  }
}

+ (NSString*)navigableAccessibilityIdentifierForToggleType:
    (CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kMostVisited:
      return kCustomizationToggleMostVisitedNavigableIdentifier;
    case CustomizationToggleType::kMagicStack:
      return kCustomizationToggleMagicStackNavigableIdentifier;
    case CustomizationToggleType::kDiscover:
      return kCustomizationToggleDiscoverNavigableIdentifier;
  }
}

+ (CustomizationMenuPage)menuPageForToggleType:(CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kMagicStack:
      return CustomizationMenuPage::kMagicStack;
    case CustomizationToggleType::kDiscover:
      return CustomizationMenuPage::kDiscover;
    default:
      return CustomizationMenuPage::kUnknown;
  }
}

+ (NSString*)titleForLinkType:(CustomizationLinkType)type {
  switch (type) {
    case CustomizationLinkType::kFollowing:
      return l10n_util::GetNSString(IDS_IOS_FEED_MANAGEMENT_FOLLOWING_TEXT);
    case CustomizationLinkType::kHidden:
      return l10n_util::GetNSString(IDS_IOS_FEED_MANAGEMENT_HIDDEN_TEXT);
    case CustomizationLinkType::kActivity:
      return l10n_util::GetNSString(IDS_IOS_FEED_MANAGEMENT_ACTIVITY_TEXT);
    case CustomizationLinkType::kLearnMore:
      return l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_MENU_LEARN_MORE_ITEM);
  }
}

+ (NSString*)subtitleForLinkType:(CustomizationLinkType)type {
  switch (type) {
    case CustomizationLinkType::kFollowing:
      return l10n_util::GetNSString(IDS_IOS_FEED_MANAGEMENT_FOLLOWING_DETAIL);
    case CustomizationLinkType::kHidden:
      return l10n_util::GetNSString(IDS_IOS_FEED_MANAGEMENT_HIDDEN_DETAIL);
    case CustomizationLinkType::kActivity:
      return l10n_util::GetNSString(IDS_IOS_FEED_MANAGEMENT_ACTIVITY_DETAIL);
    case CustomizationLinkType::kLearnMore:
      return nil;
  }
}

+ (NSString*)accessibilityIdentifierForLinkType:(CustomizationLinkType)type {
  switch (type) {
    case CustomizationLinkType::kFollowing:
      return kCustomizationLinkFollowingIdentifier;
    case CustomizationLinkType::kHidden:
      return kCustomizationLinkHiddenIdentifier;
    case CustomizationLinkType::kActivity:
      return kCustomizationLinkActivityIdentifier;
    case CustomizationLinkType::kLearnMore:
      return kCustomizationLinkLearnMoreIdentifier;
  }
}

+ (NSString*)headerTextForPage:(CustomizationMenuPage)page {
  switch (page) {
    case CustomizationMenuPage::kMain:
      return @"Main page text";
    case CustomizationMenuPage::kDiscover:
      return @"(PLACEHOLDER) A custom feed made for you by Google. Inlcuding "
             @"News, Sports and Weather.";
    case CustomizationMenuPage::kMagicStack:
      return @"Magic stack page text";
    case CustomizationMenuPage::kUnknown:
      NOTREACHED_NORETURN();
  }
}

+ (NSString*)navigationBarTitleForPage:(CustomizationMenuPage)page {
  switch (page) {
    case CustomizationMenuPage::kMain:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAIN_PAGE_NAVIGATION_TITLE);
    case CustomizationMenuPage::kDiscover:
      return @"(TODO) Discover Feed";
    case CustomizationMenuPage::kMagicStack:
      return @"(TODO) Cards";
    case CustomizationMenuPage::kUnknown:
      NOTREACHED_NORETURN();
  }
}

+ (BOOL)doesTypeHaveSubmenu:(CustomizationToggleType)type {
  return [HomeCustomizationHelper menuPageForToggleType:type] !=
         CustomizationMenuPage::kUnknown;
}

@end
