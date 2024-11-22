// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/tips_magic_stack_mediator.h"

#import <vector>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/time/time.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/power_bookmarks/core/power_bookmark_utils.h"
#import "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_magic_stack_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_consumer_source.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_state.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_prefs.h"
#import "net/traffic_annotation/network_traffic_annotation.h"
#import "url/gurl.h"
#import "url/url_util.h"

using segmentation_platform::TipIdentifier;

@interface TipsMagicStackMediator () <PrefObserverDelegate,
                                      TipsModuleAudience,
                                      TipsModuleConsumerSource>
@end

@implementation TipsMagicStackMediator {
  // The profile Pref service.
  raw_ptr<PrefService> _profilePrefService;

  // Registrar for user Pref changes notifications.
  PrefChangeRegistrar _profilePrefChangeRegistrar;

  // Bridge to listen to Pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;

  // The shopping service used to fetch product images for shopping tips.
  raw_ptr<commerce::ShoppingService> _shoppingService;

  // The image fetcher used to download product images.
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;

  // The tips module consumer.
  id<TipsMagicStackConsumer> _consumer;

  // The bookmark model used to check if a shoping page is bookmarked.
  raw_ptr<bookmarks::BookmarkModel> _bookmarkModel;
}

- (instancetype)initWithIdentifier:(TipIdentifier)identifier
                profilePrefService:(PrefService*)profilePrefService
                   shoppingService:(commerce::ShoppingService*)shoppingService
                     bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                      imageFetcher:
                          (std::unique_ptr<image_fetcher::ImageDataFetcher>)
                              imageFetcher {
  self = [super init];

  if (self) {
    CHECK(profilePrefService);
    CHECK(shoppingService);
    CHECK(bookmarkModel);
    CHECK(imageFetcher);

    _profilePrefService = profilePrefService;
    _state = [[TipsModuleState alloc] initWithIdentifier:identifier];
    _state.audience = self;
    _state.consumerSource = self;
    _shoppingService = shoppingService;
    _bookmarkModel = bookmarkModel;
    _imageFetcher = std::move(imageFetcher);

    if (!_prefObserverBridge) {
      _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);

      _profilePrefChangeRegistrar.Init(profilePrefService);

      _prefObserverBridge->ObserveChangesForPreference(
          (IsHomeCustomizationEnabled()
               ? prefs::kHomeCustomizationMagicStackTipsEnabled
               : tips_prefs::kTipsInMagicStackDisabledPref),
          &_profilePrefChangeRegistrar);
    }
  }

  return self;
}

- (void)disconnect {
  _shoppingService = nil;
  _imageFetcher = nullptr;
  _bookmarkModel = nullptr;
  _consumer = nil;

  if (_prefObserverBridge) {
    _profilePrefChangeRegistrar.RemoveAll();
    _prefObserverBridge.reset();
  }
}

- (void)reconfigureWithTipIdentifier:(TipIdentifier)identifier {
  _state = [[TipsModuleState alloc] initWithIdentifier:identifier];
  _state.audience = self;
  _state.consumerSource = self;

  if (_state.identifier == TipIdentifier::kLensShop &&
      TipsLensShopExperimentTypeEnabled() ==
          TipsLensShopExperimentType::kWithProductImage) {
    [self fetchImage];
  }
}

- (void)disableModule {
  tips_prefs::DisableTipsInMagicStack(_profilePrefService);
}

- (void)removeModuleWithCompletion:(ProceduralBlock)completion {
  [self.delegate removeTipsModuleWithCompletion:completion];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (tips_prefs::IsTipsInMagicStackDisabled(_profilePrefService)) {
    [self.delegate removeTipsModuleWithCompletion:nil];
  }
}

#pragma mark - TipsModuleConsumerSource

- (void)addConsumer:(id<TipsMagicStackConsumer>)consumer {
  _consumer = consumer;
}

#pragma mark - TipsModuleAudience

- (void)didSelectTip:(TipIdentifier)tip {
  [self.presentationAudience didSelectTip:tip];
}

#pragma mark - Private

// Fetches the product image for the most recently subscribed product from
// the shopping service and stores it in `_state.productImageData`.
- (void)fetchImage {
  std::vector<const bookmarks::BookmarkNode*> nodes =
      _shoppingService->GetAllShoppingBookmarks();

  GURL mostRecentSubscriptionProductURL =
      [self getMostRecentSubscriptionProductURL:nodes];

  __weak TipsMagicStackMediator* weakSelf = self;

  _imageFetcher->FetchImageData(
      mostRecentSubscriptionProductURL,
      base::BindOnce(^(const std::string& imageData,
                       const image_fetcher::RequestMetadata& metadata) {
        [weakSelf onImageFetchedResult:imageData];
      }),
      NO_TRAFFIC_ANNOTATION_YET);
}

// Returns the URL of the lead image for the most recently subscribed product
// in the given list of `subscriptions`. If no subscriptions have a lead
// image, returns an empty GURL.
- (GURL)getMostRecentSubscriptionProductURL:
    (std::vector<const bookmarks::BookmarkNode*>)subscriptions {
  GURL mostRecentSubscriptionProductURL;
  int64_t mostRecentSubscriptionTime = 0;

  for (const bookmarks::BookmarkNode* bookmark : subscriptions) {
    std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
        power_bookmarks::GetNodePowerBookmarkMeta(_bookmarkModel, bookmark);

    if (!meta || !meta->has_shopping_specifics()) {
      continue;
    }

    const power_bookmarks::ShoppingSpecifics specifics =
        meta->shopping_specifics();

    if (mostRecentSubscriptionProductURL.is_empty() ||
        mostRecentSubscriptionTime <
            specifics.last_subscription_change_time()) {
      mostRecentSubscriptionProductURL = GURL(meta->lead_image().url());
      mostRecentSubscriptionTime = specifics.last_subscription_change_time();
    }
  }

  return mostRecentSubscriptionProductURL;
}

// Called when the image data has been fetched. Updates the
// `_state.productImageData` with the fetched `imageData` and notifies the
// `_consumer` of the state change.
- (void)onImageFetchedResult:(const std::string&)imageData {
  NSData* data = [NSData dataWithBytes:imageData.data()
                                length:imageData.size()];

  if (data.length == 0) {
    return;
  }

  _state.productImageData = data;

  [_consumer tipsStateDidChange:_state];
}

@end
