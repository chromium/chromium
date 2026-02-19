// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module_contents_factory.h"

#import "base/notreached.h"
#import "ios/chrome/browser/content_suggestions/app_bundle_promo/ui/app_bundle_promo_config.h"
#import "ios/chrome/browser/content_suggestions/default_browser/ui/default_browser_config.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module_content_view_delegate.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_collection_view.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_stack_view.h"
#import "ios/chrome/browser/content_suggestions/price_tracking_promo/ui/price_tracking_promo_config.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_state.h"
#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_view.h"
#import "ios/chrome/browser/content_suggestions/send_tab_to_self/ui/send_tab_promo_config.h"
#import "ios/chrome/browser/content_suggestions/set_up_list/coordinator/set_up_list_mediator.h"
#import "ios/chrome/browser/content_suggestions/set_up_list/public/set_up_list_constants.h"
#import "ios/chrome/browser/content_suggestions/set_up_list/public/set_up_list_utils.h"
#import "ios/chrome/browser/content_suggestions/set_up_list/ui/set_up_list_config.h"
#import "ios/chrome/browser/content_suggestions/set_up_list/ui/set_up_list_consumer.h"
#import "ios/chrome/browser/content_suggestions/set_up_list/ui/set_up_list_consumer_source.h"
#import "ios/chrome/browser/content_suggestions/set_up_list/ui/set_up_list_item_view.h"
#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_data.h"
#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_item.h"
#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_price_tracking_view.h"
#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_view.h"
#import "ios/chrome/browser/content_suggestions/shortcuts/ui/shortcuts_action_item.h"
#import "ios/chrome/browser/content_suggestions/shortcuts/ui/shortcuts_commands.h"
#import "ios/chrome/browser/content_suggestions/shortcuts/ui/shortcuts_config.h"
#import "ios/chrome/browser/content_suggestions/shortcuts/ui/shortcuts_tile_view.h"
#import "ios/chrome/browser/content_suggestions/tab_resumption/ui/tab_resumption_item.h"
#import "ios/chrome/browser/content_suggestions/tab_resumption/ui/tab_resumption_view.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_audience.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_config.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_tile_layout_util.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/multi_row_container_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/standalone_module_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@implementation MagicStackModuleContentsFactory

- (UIView*)contentViewForConfig:(MagicStackModule*)config
                traitCollection:(UITraitCollection*)traitCollection
            contentViewDelegate:
                (id<MagicStackModuleContentViewDelegate>)contentViewDelegate {
  switch (config.type) {
    case ContentSuggestionsModuleType::kMostVisited: {
      MostVisitedTilesConfig* mvtConfig =
          static_cast<MostVisitedTilesConfig*>(config);
      if (IsContentSuggestionsCustomizable()) {
        return
            [[MostVisitedTilesCollectionView alloc] initWithConfig:mvtConfig];
      }
      return [[MostVisitedTilesStackView alloc]
          initWithConfig:mvtConfig
                 spacing:ContentSuggestionsTilesHorizontalSpacing(
                             traitCollection)];
    }
    case ContentSuggestionsModuleType::kShortcuts: {
      ShortcutsConfig* shortcutsConfig = static_cast<ShortcutsConfig*>(config);
      return [self
          shortcutsStackViewForConfig:shortcutsConfig
                          tileSpacing:ContentSuggestionsTilesHorizontalSpacing(
                                          traitCollection)];
    }
    case ContentSuggestionsModuleType::kTabResumption: {
      TabResumptionItem* tabResumptionItem =
          static_cast<TabResumptionItem*>(config);
      return [self tabResumptionViewForConfig:tabResumptionItem];
    }
    case ContentSuggestionsModuleType::kSafetyCheck: {
      SafetyCheckState* safetyCheckConfig =
          static_cast<SafetyCheckState*>(config);
      return [self safetyCheckViewForConfigState:safetyCheckConfig
                             contentViewDelegate:contentViewDelegate];
    }
    case ContentSuggestionsModuleType::kPriceTrackingPromo: {
      PriceTrackingPromoConfig* priceTrackingPromoConfig =
          static_cast<PriceTrackingPromoConfig*>(config);
      return [self priceTrackingPromoViewForConfig:priceTrackingPromoConfig];
    }
    case ContentSuggestionsModuleType::kShopCard: {
      ShopCardItem* item = static_cast<ShopCardItem*>(config);
      return [self shopCardViewForConfig:item];
    }
    case ContentSuggestionsModuleType::kSendTabPromo: {
      SendTabPromoConfig* sendTabPromoConfig =
          static_cast<SendTabPromoConfig*>(config);
      return [self sendTabPromoViewForConfig:sendTabPromoConfig];
    }
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kSetUpListAllSet:
    case ContentSuggestionsModuleType::kSetUpListNotifications: {
      SetUpListConfig* setUpListConfig = static_cast<SetUpListConfig*>(config);
      return [self setUpListViewForConfig:setUpListConfig];
    }
    case ContentSuggestionsModuleType::kTipsWithProductImage:
    case ContentSuggestionsModuleType::kTips: {
      TipsModuleConfig* tipsConfig = static_cast<TipsModuleConfig*>(config);
      return [self tipsViewForConfig:tipsConfig
                 contentViewDelegate:contentViewDelegate];
    }
    case ContentSuggestionsModuleType::kAppBundlePromo: {
      AppBundlePromoConfig* appBundlePromoConfig =
          static_cast<AppBundlePromoConfig*>(config);
      return [self appBundlePromoViewForConfig:appBundlePromoConfig];
    }
    case ContentSuggestionsModuleType::kDefaultBrowser: {
      DefaultBrowserConfig* defaultBrowserConfig =
          static_cast<DefaultBrowserConfig*>(config);
      return [self defaultBrowserViewForConfig:defaultBrowserConfig];
    }
    default:
      NOTREACHED();
  }
}

#pragma mark - Private

- (UIView*)shortcutsStackViewForConfig:(ShortcutsConfig*)shortcutsConfig
                           tileSpacing:(CGFloat)spacing {
  NSMutableArray* shortcutsViews = [NSMutableArray array];
  for (ShortcutsActionItem* item in shortcutsConfig.shortcutItems) {
    ContentSuggestionsShortcutTileView* view =
        [[ContentSuggestionsShortcutTileView alloc] initWithConfiguration:item];
    [shortcutsViews addObject:view];
  }
  UIStackView* shortcutsStackView = [[UIStackView alloc] init];
  shortcutsStackView.axis = UILayoutConstraintAxisHorizontal;
  shortcutsStackView.distribution = UIStackViewDistributionFillEqually;
  shortcutsStackView.spacing = spacing;
  shortcutsStackView.alignment = UIStackViewAlignmentTop;
  NSUInteger index = 0;
  for (ContentSuggestionsShortcutTileView* view in shortcutsViews) {
    view.accessibilityIdentifier = [NSString
        stringWithFormat:
            @"%@%li", kContentSuggestionsShortcutsAccessibilityIdentifierPrefix,
            index];
    UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:shortcutsConfig.commandHandler
                action:@selector(shortcutsTapped:)];
    [view addGestureRecognizer:tapRecognizer];
    view.tapRecognizer = tapRecognizer;
    [shortcutsStackView addArrangedSubview:view];
    index++;
  }
  return shortcutsStackView;
}

- (UIView*)tabResumptionViewForConfig:(TabResumptionItem*)tabResumptionItem {
  if (tabResumptionItem.shopCardData.shopCardItemType ==
      ShopCardItemType::kPriceTrackableProductOnTab) {
    ShopCardPriceTrackingView* shopCardPriceTrackingView =
        [[ShopCardPriceTrackingView alloc] initWithItem:tabResumptionItem];
    shopCardPriceTrackingView.commandHandler = tabResumptionItem.commandHandler;
    return shopCardPriceTrackingView;
  } else {
    TabResumptionView* tabResumptionView =
        [[TabResumptionView alloc] initWithItem:tabResumptionItem];
    tabResumptionView.commandHandler = tabResumptionItem.commandHandler;
    return tabResumptionView;
  }
}

- (UIView*)priceTrackingPromoViewForConfig:
    (PriceTrackingPromoConfig*)priceTrackingPromoConfig {
  StandaloneModuleView* view =
      [[StandaloneModuleView alloc] initWithFrame:CGRectZero];
  [view configureView:priceTrackingPromoConfig];
  view.tapDelegate = priceTrackingPromoConfig;
  return view;
}

- (UIView*)shopCardViewForConfig:(ShopCardItem*)shopCardItem {
  ShopCardModuleView* view =
      [[ShopCardModuleView alloc] initWithFrame:CGRectZero];
  view.commandHandler = shopCardItem.shopCardHandler;
  [view configureView:shopCardItem];
  return view;
}

- (UIView*)safetyCheckViewForConfigState:(SafetyCheckState*)state
                     contentViewDelegate:
                         (id<MagicStackModuleContentViewDelegate>)
                             contentViewDelegate {
  SafetyCheckView* safetyCheckView =
      [[SafetyCheckView alloc] initWithState:state
                         contentViewDelegate:contentViewDelegate];
  return safetyCheckView;
}

- (UIView*)sendTabPromoViewForConfig:(SendTabPromoConfig*)sendTabPromoConfig {
  StandaloneModuleView* view =
      [[StandaloneModuleView alloc] initWithFrame:CGRectZero];
  [view configureView:sendTabPromoConfig];
  view.tapDelegate = sendTabPromoConfig;
  return view;
}

- (UIView*)setUpListViewForConfig:(SetUpListConfig*)config {
  NSArray<SetUpListItemViewData*>* items = config.setUpListItems;

  if (!config.shouldShowCompactModule) {
    DCHECK([items count] == 1);
    SetUpListItemView* view = [[SetUpListItemView alloc] initWithData:items[0]];
    [config.setUpListConsumerSource addConsumer:view];
    view.commandHandler = config.commandHandler;
    return view;
  }

  NSMutableArray<SetUpListItemView*>* compactedSetUpListViews =
      [NSMutableArray array];
  for (SetUpListItemViewData* data in items) {
    SetUpListItemView* view = [[SetUpListItemView alloc] initWithData:data];
    [config.setUpListConsumerSource addConsumer:view];
    view.commandHandler = config.commandHandler;
    [compactedSetUpListViews addObject:view];
  }
  UIView* view =
      [[MultiRowContainerView alloc] initWithViews:compactedSetUpListViews];
  view.accessibilityIdentifier = set_up_list::kSetUpListContainerID;
  return view;
}

- (UIView*)tipsViewForConfig:(TipsModuleConfig*)config
         contentViewDelegate:
             (id<MagicStackModuleContentViewDelegate>)contentViewDelegate {
  TipsModuleView* view = [[TipsModuleView alloc] initWithConfig:config];
  view.contentViewDelegate = contentViewDelegate;
  return view;
}

// Returns a view for a given `AppBundlePromoConfig`.
- (UIView*)appBundlePromoViewForConfig:(AppBundlePromoConfig*)config {
  IconDetailView* view = [[IconDetailView alloc] initWithConfig:config];
  view.tapDelegate = config;
  return view;
}

// Returns a view for a given `DefaultBrowserConfig`.
- (UIView*)defaultBrowserViewForConfig:(DefaultBrowserConfig*)config {
  IconDetailView* view = [[IconDetailView alloc] initWithConfig:config];
  view.tapDelegate = config;
  return view;
}

@end
