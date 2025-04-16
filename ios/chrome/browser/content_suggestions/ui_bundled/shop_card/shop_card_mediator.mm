// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_mediator.h"

#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/metrics/field_trial_params.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/commerce/core/commerce_constants.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/payments/core/currency_formatter.h"
#import "components/power_bookmarks/core/power_bookmark_utils.h"
#import "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#import "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_metrics_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/impression_limit_service.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/impression_limit_service_observer_bridge.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_action_delegate.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_data.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_favicon_consumer.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_favicon_consumer_source.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_prefs.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

bool IsShopCardImpressionLimitsEnabled() {
  return base::FeatureList::IsEnabled(commerce::kShopCardImpressionLimits);
}

int GetImpressionLimit() {
  return base::GetFieldTrialParamByFeatureAsInt(
      commerce::kShopCard, commerce::kShopCardMaxImpressions,
      kShopCardMaxImpressions);
}

}  // namespace

@interface ShopCardMediator () <ImpressionLimitServiceObserverBridgeDelegate,
                                MagicStackModuleDelegate,
                                PrefObserverDelegate,
                                ShopCardFaviconConsumerSource>
@end

@implementation ShopCardMediator {
  raw_ptr<commerce::ShoppingService> _shoppingService;
  raw_ptr<bookmarks::BookmarkModel> _bookmarkModel;
  bool _shoppingDataForShopCardFound;
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;

  ShopCardItem* _shopCardItem;
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  raw_ptr<PrefService> _prefService;
  PrefChangeRegistrar _prefChangeRegistrar;

  raw_ptr<FaviconLoader> _faviconLoader;
  bool _faviconCallbackCalledOnce;
  id<ShopCardFaviconConsumer> _faviconConsumer;
  raw_ptr<ImpressionLimitService> _impressionLimitService;
  std::unique_ptr<ImpressionLimitServiceObserverBridge>
      _impressionLimitServiceObserverBridge;
}

- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)shoppingService
                prefService:(PrefService*)prefService
              bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
               imageFetcher:
                   (std::unique_ptr<image_fetcher::ImageDataFetcher>)fetcher
              faviconLoader:(FaviconLoader*)faviconLoader
     impressionLimitService:(ImpressionLimitService*)impressionLimitService {
  self = [super init];
  if (self) {
    _shoppingService = shoppingService;
    _prefService = prefService;
    _bookmarkModel = bookmarkModel;
    _imageFetcher = std::move(fetcher);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled,
        &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kHomeCustomizationMagicStackShopCardReviewsEnabled,
        &_prefChangeRegistrar);
    _faviconLoader = faviconLoader;
    _impressionLimitService = impressionLimitService;
    _impressionLimitServiceObserverBridge =
        std::make_unique<ImpressionLimitServiceObserverBridge>(
            self, _impressionLimitService);
  }
  return self;
}

- (void)disconnect {
  _shoppingService = nil;
  _bookmarkModel = nil;
  _imageFetcher = nil;
  _faviconLoader = nil;
  _impressionLimitService = nil;
  _impressionLimitServiceObserverBridge.reset();
}

- (void)reset {
  _shopCardItem = nil;
}

#pragma mark - ShopCardFaviconConsumerSource
- (void)addFaviconConsumer:(id<ShopCardFaviconConsumer>)consumer {
  _faviconConsumer = consumer;
}

- (void)setDelegate:(id<ShopCardMediatorDelegate>)delegate {
  _delegate = delegate;
  if (_delegate) {
    [self fetchLatestShopCardItem];
  }
}

- (void)fetchLatestShopCardItem {
  if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm1 &&
      !_prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled)) {
    return;
  }
  if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm2 &&
      !_prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackShopCardReviewsEnabled)) {
    return;
  }

  if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm1) {
    _shoppingDataForShopCardFound = false;
    __weak ShopCardMediator* weakSelf = self;

    GetAllPriceTrackedBookmarks(
        _shoppingService, _bookmarkModel,
        base::BindOnce(
            ^(std::vector<const bookmarks::BookmarkNode*> subscriptions) {
              ShopCardMediator* strongSelf = weakSelf;
              if (!strongSelf || !strongSelf.delegate) {
                return;
              }
              [strongSelf onPriceTrackedBookmarksReceived:subscriptions];
            }));
  } else if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm2) {
    // TODO(crbug.com/392971752): populate for card 2.
    _shopCardItem = [[ShopCardItem alloc] init];
  }
}

- (void)onPriceTrackedBookmarksReceived:
    (std::vector<const bookmarks::BookmarkNode*>)subscriptions {
  // Iterate through all subscriptions, find the first recent one with a drop
  // populate item.
  for (const bookmarks::BookmarkNode* bookmark : subscriptions) {
    if ([self hasReachedImpressionLimit:bookmark->url()] ||
        [self hasBeenOpened:bookmark->url()]) {
      continue;
    }
    std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
        power_bookmarks::GetNodePowerBookmarkMeta(_bookmarkModel, bookmark);
    if (!meta || !meta->has_shopping_specifics()) {
      continue;
    }
    const power_bookmarks::ShoppingSpecifics specifics =
        meta->shopping_specifics();

    if (!specifics.previous_price().has_amount_micros() ||
        specifics.previous_price().amount_micros() == 0) {
      continue;
    }

    [self populateShopCardItem:specifics bookmark:bookmark];

    GURL productImageUrl = GURL(meta->lead_image().url());
    __weak ShopCardMediator* weakSelf = self;
    _imageFetcher->FetchImageData(
        productImageUrl,
        base::BindOnce(^(const std::string& imageData,
                         const image_fetcher::RequestMetadata& metadata) {
          ShopCardMediator* strongSelf = weakSelf;
          if (!strongSelf || !strongSelf.delegate) {
            return;
          }
          [strongSelf onProductImageFetchedResult:imageData
                                       productUrl:GURL(bookmark->url())];
        }),
        NO_TRAFFIC_ANNOTATION_YET);

    break;
  }
}

- (void)populateShopCardItem:(const power_bookmarks::ShoppingSpecifics)specifics
                    bookmark:(const bookmarks::BookmarkNode*)bookmark {
  _shopCardItem = [[ShopCardItem alloc] init];
  _shopCardItem.delegate = self;
  _shopCardItem.shopCardData = [[ShopCardData alloc] init];
  _shopCardItem.commandHandler = self;
  _shopCardItem.shopCardFaviconConsumerSource = self;
  _shopCardItem.shopCardData.shopCardItemType =
      ShopCardItemType::kPriceDropForTrackedProducts;
  if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm1) {
    _shopCardItem.shouldShowSeeMore = YES;
  }
  PriceDrop priceDrop;

  std::unique_ptr<payments::CurrencyFormatter> formatter =
      std::make_unique<payments::CurrencyFormatter>(
          specifics.previous_price().currency_code(),
          GetApplicationContext()->GetApplicationLocale());

  float current_price_micros =
      static_cast<float>(specifics.current_price().amount_micros());
  float previous_price_micros =
      static_cast<float>(specifics.previous_price().amount_micros());

  priceDrop.current_price = [self GetFormattedPrice:formatter.get()
                                       price_micros:current_price_micros];
  priceDrop.previous_price = [self GetFormattedPrice:formatter.get()
                                        price_micros:previous_price_micros];
  _shopCardItem.shopCardData.priceDrop = priceDrop;
  _shopCardItem.shopCardData.productURL = bookmark->url();
  _shopCardItem.shopCardData.productTitle =
      [NSString stringWithUTF8String:specifics.title().c_str()];

  _shopCardItem.shopCardData.accessibilityString = l10n_util::GetNSStringF(
      IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_PRICE_TRACKING_ACCESSIBILITY_LABEL,
      base::SysNSStringToUTF16(
          _shopCardItem.shopCardData.priceDrop->previous_price),
      base::SysNSStringToUTF16(
          _shopCardItem.shopCardData.priceDrop->current_price),
      base::SysNSStringToUTF16(_shopCardItem.shopCardData.productTitle),
      GetHostnameFromGURL(bookmark->url()));
}

std::u16string GetHostnameFromGURL(const GURL& url) {
  return url_formatter::
      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(url);
}

- (NSString*)GetFormattedPrice:(payments::CurrencyFormatter*)formatter
                  price_micros:(long)price_micros {
  float price = static_cast<float>(price_micros) /
                static_cast<float>(commerce::kToMicroCurrency);
  formatter->SetMaxFractionalDigits(price >= 10.0 ? 0 : 2);
  return base::SysUTF16ToNSString(
      formatter->Format(base::NumberToString(price)));
}

- (void)onProductImageFetchedResult:(const std::string&)imageData
                         productUrl:(const GURL&)productUrl {
  if (!_shopCardItem) {
    _shopCardItem = [[ShopCardItem alloc] init];
    _shopCardItem.delegate = self;
  }
  _shopCardItem.shopCardFaviconConsumerSource = self;

  if (!_shopCardItem.shopCardData) {
    _shopCardItem.shopCardData = [[ShopCardData alloc] init];
  }
  NSData* data = [NSData dataWithBytes:imageData.data()
                                length:imageData.size()];
  if (data) {
    self->_shopCardItem.shopCardData.productImage = data;
  }
  [self.delegate insertShopCard];

  // Fetch favicon, regardless of whether product image available.
  __weak ShopCardMediator* weakSelf = self;
  _faviconLoader->FaviconForPageUrl(
      productUrl, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        [weakSelf onFaviconReceived:attributes];
      });
}

- (void)onFaviconReceived:(FaviconAttributes*)attributes {
  if (attributes.faviconImage && !attributes.usesDefaultImage) {
    self->_shopCardItem.shopCardData.faviconImage = attributes.faviconImage;
    if (_faviconCallbackCalledOnce) {
      [_faviconConsumer faviconCompleted:attributes.faviconImage];
    }
  }
  // Return early without calling the delegate, if callback already called.
  // Can't condition on faviconImage, because it may be null.
  if (_faviconCallbackCalledOnce) {
    return;
  }
  _faviconCallbackCalledOnce = true;
}

- (ShopCardItem*)shopCardItemToShow {
  return _shopCardItem;
}

#pragma mark - Public
- (void)disableModule {
  if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm1) {
    _prefService->SetBoolean(
        prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled, false);
  }
  UMA_HISTOGRAM_ENUMERATION(kMagicStackModuleDisabledHistogram,
                            ContentSuggestionsModuleType::kShopCard);
}

- (void)openShopCardItem:(ShopCardItem*)item {
  [self.NTPActionsDelegate shopCardOpened];
  [self.contentSuggestionsMetricsRecorder
      recordShopCardOpened:item.shopCardData];
  [self.shopCardActionDelegate openURL:item.shopCardData.productURL];
  [self.delegate removeShopCard];
  [self logEngagementForItem:item];
  [self reset];
  [self fetchLatestShopCardItem];
}

#pragma mark - MagicStackModuleDelegate

- (void)magicStackModule:(MagicStackModule*)magicStackModule
     wasDisplayedAtIndex:(NSUInteger)index {
  if (index == 0) {
    DCHECK(magicStackModule);
    [self logImpressionForItem:static_cast<ShopCardItem*>(magicStackModule)];
  }
  [self.contentSuggestionsMetricsRecorder
      recordShopCardImpression:static_cast<ShopCardItem*>(magicStackModule)
                                   .shopCardData
                       atIndex:index];
}

#pragma mark - ImpressionLimitServiceObserverBridgeDelegate
- (void)onUrlUntracked:(GURL)url {
  if (_shopCardItem && _shopCardItem.shopCardData &&
      url == _shopCardItem.shopCardData.productURL) {
    [self.delegate removeShopCard];
  }
}

#pragma mark - ShopCardMediatorDelegate

- (void)removeShopCard {
  [self disableModule];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName ==
      prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled) {
    if (_prefService->GetBoolean(
            prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled)) {
      // TODO(crbug.com/404564187) Fetch ShopCardData if ShopCardData
      // is nil, then insert the card.
      [self.delegate insertShopCard];
    } else {
      [self.delegate removeShopCard];
    }
  } else if (preferenceName ==
             prefs::kHomeCustomizationMagicStackShopCardReviewsEnabled) {
    if (_prefService->GetBoolean(
            prefs::kHomeCustomizationMagicStackShopCardReviewsEnabled)) {
      // TODO(crbug.com/404564187) Fetch ShopCardData if ShopCardData
      // is nil, then insert the card.
      [self.delegate insertShopCard];
    } else {
      [self.delegate removeShopCard];
    }
  }
}

- (void)logImpressionForItem:(ShopCardItem*)item {
  if (!_impressionLimitService || !IsShopCardImpressionLimitsEnabled()) {
    return;
  }
  _impressionLimitService->LogImpressionForURL(
      item.shopCardData.productURL,
      shop_card_prefs::kShopCardPriceDropUrlImpressions);
}

- (void)logEngagementForItem:(ShopCardItem*)item {
  if (!_impressionLimitService || !IsShopCardImpressionLimitsEnabled()) {
    return;
  }
  _impressionLimitService->LogCardEngagement(
      item.shopCardData.productURL,
      shop_card_prefs::kShopCardPriceDropUrlImpressions);
}

- (BOOL)hasReachedImpressionLimit:(const GURL&)url {
  if (!_impressionLimitService || !IsShopCardImpressionLimitsEnabled()) {
    return NO;
  }
  std::optional<int> count = _impressionLimitService->GetImpressionCount(
      url, shop_card_prefs::kShopCardPriceDropUrlImpressions);
  return count.has_value() && count.value() >= GetImpressionLimit();
}

- (BOOL)hasBeenOpened:(const GURL&)url {
  if (!_impressionLimitService || !IsShopCardImpressionLimitsEnabled()) {
    return NO;
  }
  return _impressionLimitService->HasBeenEngagedWith(
      url, shop_card_prefs::kShopCardPriceDropUrlImpressions);
}

#pragma mark - Testing category methods
- (commerce::ShoppingService*)shoppingServiceForTesting {
  return self->_shoppingService;
}

- (void)setShopCardItemForTesting:(ShopCardItem*)item {
  self->_shopCardItem = item;
}

- (void)logImpressionForItemForTesting:(ShopCardItem*)item {
  [self logImpressionForItem:item];
}

- (BOOL)hasReachedImpressionLimitForTesting:(const GURL&)url {
  return [self hasReachedImpressionLimit:url];
}

- (void)logEngagementForItemForTesting:(ShopCardItem*)item {
  [self logEngagementForItem:item];
}

- (BOOL)hasBeenOpenedForTesting:(const GURL&)url {
  return [self hasBeenOpened:url];
}

- (ShopCardItem*)shopCardItemForTesting {
  return self->_shopCardItem;
}
- (void)onUrlUntrackedForTesting:(GURL)url {
  [self onUrlUntracked:url];
}

@end
