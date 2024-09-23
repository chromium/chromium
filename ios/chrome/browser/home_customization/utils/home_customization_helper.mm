// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation HomeCustomizationHelper

+ (NSString*)titleForToggleType:(CustomizationToggleType)type {
  switch (type) {
      // Main page toggles.
    case CustomizationToggleType::kMostVisited:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MOST_VISITED_TITLE);
    case CustomizationToggleType::kMagicStack:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_TITLE);
    case CustomizationToggleType::kDiscover:
      return l10n_util::GetNSString(IDS_IOS_HOME_CUSTOMIZATION_DISCOVER_TITLE);

      // Magic Stack page toggles.
    case CustomizationToggleType::kSetUpList:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_TITLE_SET_UP_LIST);
    case CustomizationToggleType::kSafetyCheck:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_TITLE_SAFETY_CHECK);
    case CustomizationToggleType::kTapResumption:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_TITLE_TAB_RESUMPTION);
    case CustomizationToggleType::kParcelTracking:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_TITLE_PARCEL_TRACKING);
  }
}

+ (NSString*)subtitleForToggleType:(CustomizationToggleType)type {
  switch (type) {
      // Main page toggles.
    case CustomizationToggleType::kMostVisited:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MOST_VISITED_SUBTITLE);
    case CustomizationToggleType::kMagicStack:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_SUBTITLE);
    case CustomizationToggleType::kDiscover:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_DISCOVER_SUBTITLE);

      // Magic Stack page toggles.
    case CustomizationToggleType::kSetUpList:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_SUBTITLE_SET_UP_LIST);
    case CustomizationToggleType::kSafetyCheck:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_SUBTITLE_SAFETY_CHECK);
    case CustomizationToggleType::kTapResumption:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_SUBTITLE_TAB_RESUMPTION);
    case CustomizationToggleType::kParcelTracking:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_SUBTITLE_PARCEL_TRACKING);
  }
}

+ (UIImage*)iconForToggleType:(CustomizationToggleType)type {
  switch (type) {
      // Main page toggles.
    case CustomizationToggleType::kMostVisited:
      return DefaultSymbolWithPointSize(kHistorySymbol, kToggleIconPointSize);
    case CustomizationToggleType::kMagicStack:
      return DefaultSymbolWithPointSize(kMagicStackSymbol,
                                        kToggleIconPointSize);
    case CustomizationToggleType::kDiscover:
      return DefaultSymbolWithPointSize(kDiscoverFeedSymbol,
                                        kToggleIconPointSize);

      // Magic Stack page toggles.
    case CustomizationToggleType::kSetUpList:
      return DefaultSymbolWithPointSize(kListBulletClipboardSymbol,
                                        kToggleIconPointSize);
    case CustomizationToggleType::kSafetyCheck:
      return DefaultSymbolWithPointSize(kCheckmarkShieldSymbol,
                                        kToggleIconPointSize);
    case CustomizationToggleType::kTapResumption:
      return DefaultSymbolWithPointSize(kMacbookAndIPhoneSymbol,
                                        kToggleIconPointSize);
    case CustomizationToggleType::kParcelTracking:
      return DefaultSymbolWithPointSize(kShippingBoxSymbol,
                                        kToggleIconPointSize);
  }
}

+ (NSString*)accessibilityIdentifierForToggleType:
    (CustomizationToggleType)type {
  switch (type) {
      // Main page toggles.
    case CustomizationToggleType::kMostVisited:
      return kCustomizationToggleMostVisitedIdentifier;
    case CustomizationToggleType::kMagicStack:
      return kCustomizationToggleMagicStackIdentifier;
    case CustomizationToggleType::kDiscover:
      return kCustomizationToggleDiscoverIdentifier;

      // Magic Stack page toggles.
    case CustomizationToggleType::kSetUpList:
      return kCustomizationToggleSetUpListIdentifier;
    case CustomizationToggleType::kSafetyCheck:
      return kCustomizationToggleSafetyCheckIdentifier;
    case CustomizationToggleType::kTapResumption:
      return kCustomizationToggleTabResumptionIdentifier;
    case CustomizationToggleType::kParcelTracking:
      return kCustomizationToggleParcelTrackingIdentifier;
  }
}

+ (NSString*)navigableAccessibilityIdentifierForToggleType:
    (CustomizationToggleType)type {
  switch (type) {
      // Main page toggles.
    case CustomizationToggleType::kMostVisited:
      return kCustomizationToggleMostVisitedNavigableIdentifier;
    case CustomizationToggleType::kMagicStack:
      return kCustomizationToggleMagicStackNavigableIdentifier;
    case CustomizationToggleType::kDiscover:
      return kCustomizationToggleDiscoverNavigableIdentifier;

      // Magic Stack page toggles.
    case CustomizationToggleType::kSetUpList:
      return nil;
    case CustomizationToggleType::kSafetyCheck:
      return nil;
    case CustomizationToggleType::kTapResumption:
      return nil;
    case CustomizationToggleType::kParcelTracking:
      return nil;
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
      return nil;
    case CustomizationMenuPage::kDiscover:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_DISCOVER_PAGE_HEADER);
    case CustomizationMenuPage::kMagicStack:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_PAGE_HEADER);
    case CustomizationMenuPage::kUnknown:
      NOTREACHED();
  }
}

+ (NSString*)navigationBarTitleForPage:(CustomizationMenuPage)page {
  switch (page) {
    case CustomizationMenuPage::kMain:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAIN_PAGE_NAVIGATION_TITLE);
    case CustomizationMenuPage::kMagicStack:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_TITLE);
    case CustomizationMenuPage::kDiscover:
      return l10n_util::GetNSString(IDS_IOS_HOME_CUSTOMIZATION_DISCOVER_TITLE);
    case CustomizationMenuPage::kUnknown:
      NOTREACHED();
  }
}

+ (NSString*)accessibilityIdentifierForPageCollection:
    (CustomizationMenuPage)page {
  switch (page) {
    case CustomizationMenuPage::kMain:
      return kCustomizationCollectionMainIdentifier;
    case CustomizationMenuPage::kMagicStack:
      return kCustomizationCollectionMagicStackIdentifier;
    case CustomizationMenuPage::kDiscover:
      return kCustomizationCollectionDiscoverIdentifier;
    case CustomizationMenuPage::kUnknown:
      NOTREACHED();
  }
}

+ (BOOL)doesTypeHaveSubmenu:(CustomizationToggleType)type {
  return [HomeCustomizationHelper menuPageForToggleType:type] !=
         CustomizationMenuPage::kUnknown;
}

@end
