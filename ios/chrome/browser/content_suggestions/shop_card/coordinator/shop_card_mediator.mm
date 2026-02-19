// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/shop_card/coordinator/shop_card_mediator.h"

#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/metrics/field_trial_params.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/commerce/core/commerce_constants.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/pref_names.h"
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
#import "ios/chrome/browser/content_suggestions/impression_limits/model/impression_limit_service.h"
#import "ios/chrome/browser/content_suggestions/impression_limits/model/impression_limit_service_observer_bridge.h"
#import "ios/chrome/browser/content_suggestions/model/content_suggestions_metrics_constants.h"
#import "ios/chrome/browser/content_suggestions/model/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/shop_card/coordinator/shop_card_action_delegate.h"
#import "ios/chrome/browser/content_suggestions/shop_card/coordinator/shop_card_mediator_delegate.h"
#import "ios/chrome/browser/content_suggestions/shop_card/model/shop_card_prefs.h"
#import "ios/chrome/browser/content_suggestions/shop_card/public/shop_card_constants.h"
#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_config.h"
#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_data.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface ShopCardMediator () <ImpressionLimitServiceObserverBridgeDelegate,
                                MagicStackModuleDelegate,
                                PrefObserverDelegate>
@end

@implementation ShopCardMediator {
  raw_ptr<commerce::ShoppingService> _shoppingService;
  raw_ptr<bookmarks::BookmarkModel> _bookmarkModel;
  bool _shoppingDataForShopCardFound;
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;

  ShopCardConfig* _shopCardConfig;
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  raw_ptr<PrefService> _prefService;
  PrefChangeRegistrar _prefChangeRegistrar;

  raw_ptr<FaviconLoader> _faviconLoader;
  bool _faviconCallbackCalledOnce;
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
        commerce::kPriceTrackingHomeModuleEnabled, &_prefChangeRegistrar);
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
  _shopCardConfig = nil;
  _shoppingDataForShopCardFound = false;
}

- (void)setDelegate:(id<ShopCardMediatorDelegate>)delegate {
  _delegate = delegate;
  if (_delegate) {
    [self fetchLatestShopCardConfig];
  }
}

- (void)fetchLatestShopCardConfig {
  if (!_prefService->GetBoolean(commerce::kPriceTrackingHomeModuleEnabled)) {
    return;
  }

  [self fetchPriceTrackedBookmarksIfApplicable];
}

- (void)fetchPriceTrackedBookmarksIfApplicable {
  if (self->_shopCardConfig) {
    return;
  }
  [self fetchPriceTrackedBookmarks];
}

- (void)fetchPriceTrackedBookmarks {
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
}

- (void)onPriceTrackedBookmarksReceived:
    (std::vector<const bookmarks::BookmarkNode*>)subscriptions {
  if (_shoppingDataForShopCardFound) {
    // Prevent duplicate Magic Stack insertions.
    return;
  }
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

    _shoppingDataForShopCardFound = true;

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
          [strongSelf populateShopCardConfig:specifics url:bookmark->url()];
          [strongSelf onProductImageFetchedResult:imageData
                                       productUrl:GURL(bookmark->url())];
        }),
        NO_TRAFFIC_ANNOTATION_YET);

    break;
  }
}

- (void)populateShopCardConfig:
            (const power_bookmarks::ShoppingSpecifics)specifics
                           url:(const GURL&)url {
  _shopCardConfig = [[ShopCardConfig alloc] init];
  _shopCardConfig.delegate = self;
  _shopCardConfig.shopCardData = [[ShopCardData alloc] init];
  _shopCardConfig.shopCardHandler = self;
  _shopCardConfig.shopCardData.shopCardItemType =
      ShopCardItemType::kPriceDropForTrackedProducts;
  PriceDrop priceDrop;

  std::unique_ptr<payments::CurrencyFormatter> formatter =
      std::make_unique<payments::CurrencyFormatter>(
          specifics.previous_price().currency_code(),
          GetApplicationContext()->GetApplicationLocaleStorage()->Get());

  float current_price_micros =
      static_cast<float>(specifics.current_price().amount_micros());
  float previous_price_micros =
      static_cast<float>(specifics.previous_price().amount_micros());

  priceDrop.current_price = [self GetFormattedPrice:formatter.get()
                                       price_micros:current_price_micros];
  priceDrop.previous_price = [self GetFormattedPrice:formatter.get()
                                        price_micros:previous_price_micros];
  _shopCardConfig.shopCardData.priceDrop = priceDrop;
  _shopCardConfig.shopCardData.productURL = url;
  _shopCardConfig.shopCardData.productTitle =
      [NSString stringWithUTF8String:specifics.title().c_str()];

  _shopCardConfig.shopCardData.accessibilityString = l10n_util::GetNSStringF(
      IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_PRICE_TRACKING_ACCESSIBILITY_LABEL,
      base::SysNSStringToUTF16(
          _shopCardConfig.shopCardData.priceDrop->previous_price),
      base::SysNSStringToUTF16(
          _shopCardConfig.shopCardData.priceDrop->current_price),
      base::SysNSStringToUTF16(_shopCardConfig.shopCardData.productTitle),
      GetHostnameFromGURL(url));
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
  if (!_shopCardConfig) {
    _shopCardConfig = [[ShopCardConfig alloc] init];
    _shopCardConfig.delegate = self;
  }

  if (!_shopCardConfig.shopCardData) {
    _shopCardConfig.shopCardData = [[ShopCardData alloc] init];
  }
  NSData* data = [NSData dataWithBytes:imageData.data()
                                length:imageData.size()];
  if (data) {
    self->_shopCardConfig.shopCardData.productImage = data;
  }
  [self.delegate insertShopCard];

  // Fetch favicon, regardless of whether product image available.
  __weak ShopCardMediator* weakSelf = self;
  _faviconLoader->FaviconForPageUrl(
      productUrl, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false,
      ^(FaviconAttributes* attributes, bool cached) {
        [weakSelf onFaviconReceived:attributes];
      });
}

- (void)onFaviconReceived:(FaviconAttributes*)attributes {
  if (attributes.faviconImage) {
    self->_shopCardConfig.shopCardData.faviconImage = attributes.faviconImage;
    if (_faviconCallbackCalledOnce) {
      [self.delegate shopCardMediatorDidReconfigureItem];
    }
  }
  // Return early without calling the delegate, if callback already called.
  // Can't condition on faviconImage, because it may be null.
  if (_faviconCallbackCalledOnce) {
    return;
  }
  _faviconCallbackCalledOnce = true;
}

- (ShopCardConfig*)shopCardItemToShow {
  return _shopCardConfig;
}

#pragma mark - Public
- (void)disableModule {
  _prefService->SetBoolean(commerce::kPriceTrackingHomeModuleEnabled, false);
  UMA_HISTOGRAM_ENUMERATION(kMagicStackModuleDisabledHistogram,
                            ContentSuggestionsModuleType::kShopCard);
}

- (void)openShopCardItem:(ShopCardConfig*)config {
  [self.NTPActionsDelegate shopCardOpened];
  [self.contentSuggestionsMetricsRecorder
      recordShopCardOpened:config.shopCardData];
  [self.shopCardActionDelegate openURL:config.shopCardData.productURL];
  [self.delegate removeShopCard];
  [self logEngagementForItem:config];
  [self reset];
  [self fetchLatestShopCardConfig];
}

#pragma mark - MagicStackModuleDelegate

- (void)magicStackModule:(MagicStackModule*)magicStackModule
     wasDisplayedAtIndex:(NSUInteger)index {
  if (index == 0) {
    DCHECK(magicStackModule);
    [self logImpressionForItem:static_cast<ShopCardConfig*>(magicStackModule)];
  }
  [self.contentSuggestionsMetricsRecorder
      recordShopCardImpression:static_cast<ShopCardConfig*>(magicStackModule)
                                   .shopCardData
                       atIndex:index];
}

#pragma mark - ImpressionLimitServiceObserverBridgeDelegate

- (void)impressionLimitService:(ImpressionLimitService*)impressionLimitService
                 didUntrackURL:(GURL)url {
  if (_shopCardConfig && _shopCardConfig.shopCardData &&
      url == _shopCardConfig.shopCardData.productURL) {
    [self.delegate removeShopCard];
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == commerce::kPriceTrackingHomeModuleEnabled) {
    if (_prefService->GetBoolean(commerce::kPriceTrackingHomeModuleEnabled)) {
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

- (void)logImpressionForItem:(ShopCardConfig*)item {
  if (!_impressionLimitService) {
    return;
  }
  _impressionLimitService->LogImpressionForURL(
      item.shopCardData.productURL,
      shop_card_prefs::kShopCardPriceDropUrlImpressions);
}

- (void)logEngagementForItem:(ShopCardConfig*)item {
  if (!_impressionLimitService) {
    return;
  }
  _impressionLimitService->LogCardEngagement(
      item.shopCardData.productURL,
      shop_card_prefs::kShopCardPriceDropUrlImpressions);
}

- (BOOL)hasReachedImpressionLimit:(const GURL&)url {
  if (!_impressionLimitService) {
    return NO;
  }
  std::optional<int> count = _impressionLimitService->GetImpressionCount(
      url, shop_card_prefs::kShopCardPriceDropUrlImpressions);
  return count.has_value() && count.value() >= kShopCardMaxImpressions;
}

- (BOOL)hasBeenOpened:(const GURL&)url {
  if (!_impressionLimitService) {
    return NO;
  }
  return _impressionLimitService->HasBeenEngagedWith(
      url, shop_card_prefs::kShopCardPriceDropUrlImpressions);
}

#pragma mark - Testing category methods
- (commerce::ShoppingService*)shoppingServiceForTesting {
  return self->_shoppingService;
}

- (void)setShopCardConfigForTesting:(ShopCardConfig*)config {
  self->_shopCardConfig = config;
}

- (void)logImpressionForItemForTesting:(ShopCardConfig*)config {
  [self logImpressionForItem:config];
}

- (BOOL)hasReachedImpressionLimitForTesting:(const GURL&)url {
  return [self hasReachedImpressionLimit:url];
}

- (void)logEngagementForItemForTesting:(ShopCardConfig*)config {
  [self logEngagementForItem:config];
}

- (BOOL)hasBeenOpenedForTesting:(const GURL&)url {
  return [self hasBeenOpened:url];
}

- (ShopCardConfig*)shopCardConfigForTesting {
  return self->_shopCardConfig;
}
- (void)onUrlUntrackedForTesting:(GURL)url {
  [self impressionLimitService:_impressionLimitService didUntrackURL:url];
}

- (void)fetchPriceTrackedBookmarksForTesting {
  [self fetchPriceTrackedBookmarks];
}

- (void)fetchPriceTrackedBookmarksIfApplicableForTesting {
  [self fetchPriceTrackedBookmarksIfApplicable];
}

@end
