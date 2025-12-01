// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module_contents_factory.h"

#import "base/notreached.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/ui/app_bundle_promo_config.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/ui/app_bundle_promo_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_tile_layout_util.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/most_visited_tiles_collection_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/most_visited_tiles_stack_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/multi_row_container_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/default_browser/ui/default_browser_config.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/default_browser/ui/default_browser_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module_content_view_delegate.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/ui/price_tracking_promo_favicon_consumer_source.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/ui/price_tracking_promo_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/ui/price_tracking_promo_module_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/ui/safety_check_consumer_source.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/ui/safety_check_state.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/ui/safety_check_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/send_tab_to_self/ui/send_tab_promo_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/coordinator/set_up_list_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/public/set_up_list_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/public/set_up_list_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/ui/set_up_list_config.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/ui/set_up_list_consumer.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/ui/set_up_list_consumer_source.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/ui/set_up_list_item_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/ui/shop_card_data.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/ui/shop_card_favicon_consumer_source.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/ui/shop_card_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/ui/shop_card_price_tracking_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/ui/shop_card_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_action_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_commands.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_config.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_consumer_source.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_tile_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/standalone_module_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tab_resumption/ui/tab_resumption_consumer_source.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tab_resumption/ui/tab_resumption_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tab_resumption/ui/tab_resumption_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/ui/tips_module_audience.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/ui/tips_module_consumer_source.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/ui/tips_module_state.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/ui/tips_module_view.h"
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
      PriceTrackingPromoItem* item =
          static_cast<PriceTrackingPromoItem*>(config);
      return [self priceTrackingPromoViewForConfig:item];
    }
    case ContentSuggestionsModuleType::kShopCard: {
      ShopCardItem* item = static_cast<ShopCardItem*>(config);
      return [self shopCardViewForConfig:item];
    }
    case ContentSuggestionsModuleType::kSendTabPromo: {
      SendTabPromoItem* item = static_cast<SendTabPromoItem*>(config);
      return [self sendTabPromoViewForConfig:item];
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
      TipsModuleState* tipsConfig = static_cast<TipsModuleState*>(config);
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
    [shortcutsConfig.consumerSource addConsumer:view];
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
    [tabResumptionItem.consumerSource addConsumer:tabResumptionView];
    tabResumptionView.commandHandler = tabResumptionItem.commandHandler;
    return tabResumptionView;
  }
}

- (UIView*)priceTrackingPromoViewForConfig:
    (PriceTrackingPromoItem*)priceTrackingPromoItem {
  PriceTrackingPromoModuleView* view =
      [[PriceTrackingPromoModuleView alloc] initWithFrame:CGRectZero];
  [priceTrackingPromoItem.priceTrackingPromoFaviconConsumerSource
      addConsumer:view];
  view.commandHandler = priceTrackingPromoItem.commandHandler;
  [view configureView:priceTrackingPromoItem];
  return view;
}

- (UIView*)shopCardViewForConfig:(ShopCardItem*)shopCardItem {
  ShopCardModuleView* view =
      [[ShopCardModuleView alloc] initWithFrame:CGRectZero];
  [shopCardItem.shopCardFaviconConsumerSource addFaviconConsumer:view];
  view.commandHandler = shopCardItem.commandHandler;
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
  safetyCheckView.audience = state.audience;
  [state.safetyCheckConsumerSource addConsumer:safetyCheckView];
  return safetyCheckView;
}

- (UIView*)sendTabPromoViewForConfig:(SendTabPromoItem*)sendTabPromoItem {
  StandaloneModuleView* view =
      [[StandaloneModuleView alloc] initWithFrame:CGRectZero];
  view.delegate = sendTabPromoItem.standaloneDelegate;
  [view configureView:sendTabPromoItem];
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

- (UIView*)tipsViewForConfig:(TipsModuleState*)state
         contentViewDelegate:
             (id<MagicStackModuleContentViewDelegate>)contentViewDelegate {
  TipsModuleView* view = [[TipsModuleView alloc] initWithState:state];

  view.audience = state.audience;
  view.contentViewDelegate = contentViewDelegate;
  [state.consumerSource addConsumer:view];

  return view;
}

// Returns an `AppBundlePromoView` for a given `AppBundlePromoConfig`.
- (UIView*)appBundlePromoViewForConfig:(AppBundlePromoConfig*)config {
  AppBundlePromoView* view = [[AppBundlePromoView alloc] initWithConfig:config];
  view.audience = config.audience;

  return view;
}

// Returns a `DefaultBrowserView` for a given `DefaultBrowserConfig`.
- (UIView*)defaultBrowserViewForConfig:(DefaultBrowserConfig*)config {
  DefaultBrowserView* view = [[DefaultBrowserView alloc] initWithConfig:config];
  view.commandHandler = config.commandHandler;

  return view;
}

@end
