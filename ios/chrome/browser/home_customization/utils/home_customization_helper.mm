// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"

#import "base/containers/contains.h"
#import "base/notreached.h"
#import "components/commerce/core/commerce_feature_list.h"
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
          IDS_IOS_CONTENT_SUGGESTIONS_MOST_VISITED_MODULE_TITLE);
    case CustomizationToggleType::kMagicStack:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_TITLE);
    case CustomizationToggleType::kDiscover:
      return l10n_util::GetNSString(IDS_IOS_HOME_CUSTOMIZATION_DISCOVER_TITLE);

      // Magic Stack page toggles.
    case CustomizationToggleType::kSafetyCheck:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_TITLE_SAFETY_CHECK);
    case CustomizationToggleType::kTapResumption:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_TITLE_TAB_RESUMPTION);
    case CustomizationToggleType::kTips:
      return l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_TITLE);
    case CustomizationToggleType::kShopCard:
      return l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_PRICE_TRACKING_CUSTOMIZE_CARDS);
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
    case CustomizationToggleType::kSafetyCheck:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_SUBTITLE_SAFETY_CHECK);
    case CustomizationToggleType::kTapResumption:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_SUBTITLE_TAB_RESUMPTION);
    case CustomizationToggleType::kTips:
      return l10n_util::GetNSString(
          IDS_IOS_HOME_CUSTOMIZATION_MAGIC_STACK_SUBTITLE_TIPS);
    case CustomizationToggleType::kShopCard:
      return l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_PRICE_TRACKING_CUSTOMIZE_CARDS_SUBTITLE);
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
    case CustomizationToggleType::kSafetyCheck:
      return DefaultSymbolWithPointSize(kCheckmarkShieldSymbol,
                                        kToggleIconPointSize);
    case CustomizationToggleType::kTapResumption:
      return DefaultSymbolWithPointSize(kMacbookAndIPhoneSymbol,
                                        kToggleIconPointSize);
    case CustomizationToggleType::kTips:
      return DefaultSymbolWithPointSize(kListBulletClipboardSymbol,
                                        kToggleIconPointSize);
    case CustomizationToggleType::kShopCard: {
      UIImageSymbolConfiguration* fallbackImageConfig =
          [UIImageSymbolConfiguration
              configurationWithWeight:UIImageSymbolWeightLight];
      return CustomSymbolWithConfiguration(kDownTrendSymbol,
                                           fallbackImageConfig);
      NOTREACHED();
    }
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
    case CustomizationToggleType::kSafetyCheck:
      return kCustomizationToggleSafetyCheckIdentifier;
    case CustomizationToggleType::kTapResumption:
      return kCustomizationToggleTabResumptionIdentifier;
    case CustomizationToggleType::kTips:
      return kCustomizationToggleTipsIdentifier;
    case CustomizationToggleType::kShopCard:
      return kCustomizationToggleShopCardPriceTrackingIdentifier;
      NOTREACHED();
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
    case CustomizationToggleType::kSafetyCheck:
      return nil;
    case CustomizationToggleType::kTapResumption:
      return nil;
    case CustomizationToggleType::kTips:
      return nil;
    case CustomizationToggleType::kShopCard:
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
    case CustomizationLinkType::kEnterpriseLearnMore:
      return nil;
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
    case CustomizationLinkType::kEnterpriseLearnMore:
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
    case CustomizationLinkType::kEnterpriseLearnMore:
      return nil;
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
