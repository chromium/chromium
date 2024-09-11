// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_price_tracking_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "components/commerce/core/commerce_constants.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/payments/core/currency_formatter.h"
#import "components/power_bookmarks/core/power_bookmark_utils.h"
#import "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#import "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#import "ios/chrome/browser/price_insights/coordinator/price_insights_consumer.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/price_notifications_commands.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_alert_presenter.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_consumer.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace {
// The histogram used to record a product's new tracking state when a user
// initates a state change.
const char kPriceTrackingStatusHistogram[] =
    "Commerce.PriceTracking.IOS.PriceTracking.ProductStatus";

// The histogram used to record a product's new tracking state when a user
// initates a state change.
const char kPriceInsightsTrackingStatusHistogram[] =
    "Commerce.PriceTracking.IOS.PriceInsights.ProductStatus";

// This enum is used to represent the different tracking states a product can
// observe.
enum class PriceNotificationProductStatus {
  kTrack,
  kUntrack,
  kMaxValue = kUntrack
};

// This enum is used to represent the different sources of tracking a product.
enum class PriceNotificationTrackingSource {
  kPriceTracking,
  kPriceInsights,
  kMaxValue = kPriceInsights
};

}  // namespace

using PriceNotificationItems =
    NSMutableArray<PriceNotificationsTableViewItem*>*;

@interface PriceNotificationsPriceTrackingMediator () {
  // The service responsible for fetching a product's image data.
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;
}
// The service responsible for interacting with commerce's price data
// infrastructure.
@property(nonatomic, assign) commerce::ShoppingService* shoppingService;
// The service responsible for managing bookmarks.
@property(nonatomic, readonly) bookmarks::BookmarkModel* bookmarkModel;
// The current browser state's webstate.
@property(nonatomic, assign) base::WeakPtr<web::WebState> webState;
// The product data for the product contained on the site the user is currently
// viewing.
@property(nonatomic, assign) std::optional<commerce::ProductInfo>
    currentSiteProductInfo;
// The service responsible for updating the user's chrome-level push
// notification permissions for Price Tracking.
@property(nonatomic, assign) PushNotificationService* pushNotificationService;

@end

@implementation PriceNotificationsPriceTrackingMediator

- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)service
              bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
               imageFetcher:
                   (std::unique_ptr<image_fetcher::ImageDataFetcher>)fetcher
                   webState:(base::WeakPtr<web::WebState>)webState
    pushNotificationService:(PushNotificationService*)pushNotificationService {
  self = [super init];
  if (self) {
    DCHECK(service);
    DCHECK(bookmarkModel);
    DCHECK(fetcher);
    DCHECK(webState);
    DCHECK(pushNotificationService);
    _shoppingService = service;
    _bookmarkModel = bookmarkModel;
    _imageFetcher = std::move(fetcher);
    _webState = webState;
    _pushNotificationService = pushNotificationService;
  }

  return self;
}

- (void)setConsumer:(id<PriceNotificationsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [self fetchPriceTrackingData];
}

- (void)setPriceInsightsConsumer:(id<PriceInsightsConsumer>)consumer {
  if (_priceInsightsConsumer == consumer) {
    return;
  }

  _priceInsightsConsumer = consumer;
}

#pragma mark - PriceNotificationsMutator

- (void)trackItem:(PriceNotificationsTableViewItem*)item {
  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;
  [self presentNotificationPermission:^(BOOL granted, BOOL promptShown,
                                        NSError* error) {
    if (!error && !promptShown && !granted) {
      // This callback can be executed on a background thread, make sure the UI
      // is displayed on the main thread.
      dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf.presenter presentPushNotificationPermissionAlert];
      });
    } else if (!error && promptShown && granted) {
      // This callback can be executed on a background thread causing this
      // function to fail. Thus, the invocation is scheduled to run on the main
      // thread.
      dispatch_async(dispatch_get_main_queue(), ^{
        weakSelf.pushNotificationService->SetPreference(
            weakSelf.gaiaID, PushNotificationClientId::kCommerce, true);
      });
    }
  }];

  [self trackForURL:item.entryURL
                  title:item.title
      completionHandler:^(bool success) {
        [weakSelf didTrackItem:item successfully:success];
      }];
}

- (void)stopTrackingItem:(PriceNotificationsTableViewItem*)item {
  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;
  [self stopTrackingForURL:item.entryURL
      withCompletionHandler:^(bool success) {
        if (!success) {
          [weakSelf.presenter presentStopPriceTrackingErrorAlertForItem:item];
          return;
        }
        [weakSelf didStopTrackingItem:item];
      }];
}

- (void)navigateToWebpageForItem:(PriceNotificationsTableViewItem*)item {
  DCHECK(item.tracking);
  [self navigateToWebpageForURL:item.entryURL
                    disposition:WindowOpenDisposition::CURRENT_TAB];
  [self.handler hidePriceNotifications];
}

- (void)navigateToBookmarks {
  [self.handler hidePriceNotifications];
  GURL URL = _webState->GetLastCommittedURL();
  [self.bookmarksHandler openToExternalBookmark:URL];
}

#pragma mark - PriceInsightsMutator

- (void)tryPriceInsightsTrackItem:(PriceInsightsItem*)item {
  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;

  auto callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(^(BOOL granted, BOOL promptShown, NSError* error) {
        [weakSelf onNotificationPermissionRequestForItem:item
                                       permissionGranted:granted
                                             promptShown:promptShown
                                                   error:error];
      }));

  [self
      presentNotificationPermission:base::CallbackToBlock(std::move(callback))];
}

- (void)priceInsightsTrackItem:(PriceInsightsItem*)item
          notificationsGranted:(BOOL)granted
                showCompletion:(BOOL)showCompletion {
  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;
  [self trackForURL:item.productURL
                  title:item.title
      completionHandler:^(bool success) {
        [weakSelf onPriceInsightsTrackItem:item
                                   success:success
                         permissionGranted:granted
                            showCompletion:showCompletion];
      }];
}

- (void)priceInsightsStopTrackingItem:(PriceInsightsItem*)item {
  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;
  [self stopTrackingForURL:item.productURL
                  clusterId:item.clusterId
      withCompletionHandler:^(bool success) {
        [weakSelf onPriceInsightsStopTrackingItem:item success:success];
      }];
}

- (void)priceInsightsNavigateToWebpageForItem:(PriceInsightsItem*)item {
  DCHECK(item.buyingOptionsURL.is_valid());
  [self navigateToWebpageForURL:item.buyingOptionsURL
                    disposition:WindowOpenDisposition::NEW_FOREGROUND_TAB];
  [self.priceInsightsConsumer
      didStartNavigationToWebpageWithPriceBucket:item.priceBucket];
}

#pragma mark - Private

// This function fetches the product data for the item on the currently visible
// page and populates the data into the Price Notifications UI.
- (void)fetchTrackableItemDataAtSite:(const GURL&)URL {
  if ([self isPriceTrackingURL:URL]) {
    [self.consumer setTrackableItem:nil currentlyTracking:YES];
    return;
  }

  [self displayProduct:self.currentSiteProductInfo fromSite:URL];
}

// Creates a `PriceNotificationsTableViewItem` object and sends the newly
// created object to the Price Notifications UI.
- (void)displayProduct:(const std::optional<commerce::ProductInfo>&)productInfo
              fromSite:(const GURL&)URL {
  if (!commerce::CanTrackPrice(productInfo)) {
    [self.consumer setTrackableItem:nil currentlyTracking:NO];
    return;
  }

  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;

  PriceNotificationsTableViewItem* item =
      [self createPriceNotificationTableViewItem:NO
                                 fromProductInfo:productInfo
                                           atURL:URL];
  self.shoppingService->IsSubscribed(
      commerce::BuildUserSubscriptionForClusterId(
          productInfo->product_cluster_id.value()),
      base::BindOnce(^(bool isTracked) {
        [weakSelf.consumer setTrackableItem:item currentlyTracking:isTracked];
      }));

  // Fetches the current item's trackable image.
  _imageFetcher->FetchImageData(
      productInfo->image_url,
      base::BindOnce(^(const std::string& imageData,
                       const image_fetcher::RequestMetadata& metadata) {
        [weakSelf updateItem:item withImage:imageData];
      }),
      NO_TRAFFIC_ANNOTATION_YET);
}

// Adds the downloaded product image to the `PriceNotificationsTableViewItem`
// and sends the amended item to the Price Notifications UI.
- (void)updateItem:(PriceNotificationsTableViewItem*)item
         withImage:(const std::string&)imageData {
  NSData* data = [NSData dataWithBytes:imageData.data()
                                length:imageData.size()];
  if (data) {
    item.productImage = [UIImage imageWithData:data
                                         scale:[UIScreen mainScreen].scale];
  }

  [self.consumer reconfigureCellsForItems:@[ item ]];
}

// Creates a localized price string.
- (NSString*)extractFormattedCurrentPrice:(BOOL)forCurrentPrice
                          fromProductInfo:
                              (const std::optional<commerce::ProductInfo>&)
                                  productInfo {
  if (!productInfo) {
    return nil;
  }

  if (!forCurrentPrice && !productInfo->previous_amount_micros) {
    return nil;
  }

  int64_t amountMicro = forCurrentPrice
                            ? productInfo->amount_micros
                            : productInfo->previous_amount_micros.value();
  float price = static_cast<float>(amountMicro) /
                static_cast<float>(commerce::kToMicroCurrency);
  payments::CurrencyFormatter formatter(productInfo->currency_code,
                                        productInfo->country_code);

  formatter.SetMaxFractionalDigits(2);
  return base::SysUTF16ToNSString(
      formatter.Format(base::NumberToString(price)));
}

// This function handles the response from the user attempting to subscribe to
// an item with the ShoppingService.
- (void)didTrackItem:(PriceNotificationsTableViewItem*)trackableItem
        successfully:(BOOL)success {
  if (!success) {
    [self.presenter presentStartPriceTrackingErrorAlertForItem:trackableItem];
    return;
  }

  trackableItem.tracking = YES;
  [self.consumer reconfigureCellsForItems:@[ trackableItem ]];
  [self.consumer didStartPriceTrackingForItem:trackableItem];

  [self recordProductStatusFromSource:PriceNotificationTrackingSource::
                                          kPriceTracking
                               status:PriceNotificationProductStatus::kTrack];
}

// This function handles the response from the user attempting to unsubscribe to
// an item with the ShoppingService.
- (void)didStopTrackingItem:(PriceNotificationsTableViewItem*)item {
  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;
  self.shoppingService->GetProductInfoForUrl(
      item.entryURL,
      base::BindOnce(^(
          const GURL& productURL,
          const std::optional<const commerce::ProductInfo>& productInfo) {
        PriceNotificationsPriceTrackingMediator* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }

        BOOL isProductOnCurrentSite =
            [strongSelf isCurrentSiteEqualToProductInfo:productInfo];
        [strongSelf.consumer didStopPriceTrackingItem:item
                                        onCurrentSite:isProductOnCurrentSite];
      }));

  [self recordProductStatusFromSource:PriceNotificationTrackingSource::
                                          kPriceTracking
                               status:PriceNotificationProductStatus::kUntrack];
}

// This function fetches the product data for the items the user has subscribed
// to and populates the data into the Price Notifications UI.
- (void)fetchTrackedItems {
  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;
  self.shoppingService->GetAllPriceTrackedBookmarks(
      base::BindOnce(
          ^(std::vector<const bookmarks::BookmarkNode*> subscribedItems) {
            if (!weakSelf) {
              return;
            }
            for (const bookmarks::BookmarkNode* bookmark : subscribedItems) {
              std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
                  power_bookmarks::GetNodePowerBookmarkMeta(
                      weakSelf.bookmarkModel, bookmark);
              if (!meta || !meta->has_shopping_specifics()) {
                continue;
              }
              const power_bookmarks::ShoppingSpecifics specifics =
                  meta->shopping_specifics();
              // To build the PriceNotificationTableViewItem for product on
              // current page which are not being tracked, we have to use its
              // ProductInfo. To avoid duplicate APIs, here we also convert
              // BookmarkMeta to ProductInfo to build the
              // PriceNotificationTableViewItem for tracked products, instead of
              // passing BookmarkMeta directly.
              std::optional<commerce::ProductInfo> info;
              info.emplace();
              info->title = specifics.title();
              info->image_url = GURL(meta->lead_image().url());
              if (specifics.has_product_cluster_id()) {
                info->product_cluster_id.emplace(
                    specifics.product_cluster_id());
              }
              if (specifics.has_offer_id()) {
                info->offer_id.emplace(specifics.offer_id());
              }
              info->currency_code = specifics.current_price().currency_code();
              info->amount_micros = specifics.current_price().amount_micros();
              info->country_code = specifics.country_code();
              if (specifics.has_previous_price() &&
                  specifics.previous_price().amount_micros() >
                      specifics.current_price().amount_micros()) {
                info->previous_amount_micros.emplace(
                    specifics.previous_price().amount_micros());
              }
              [weakSelf addTrackedItem:info fromSite:bookmark->url()];
            }
          }));
}

// Retrieves the product data for the items the user has subscribed to and the
// item contained on the webpage the user is currently viewing.
- (void)fetchPriceTrackingData {
  const GURL& currentSiteURL = self.webState->GetVisibleURL();
  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;
  self.shoppingService->GetProductInfoForUrl(
      currentSiteURL,
      base::BindOnce(
          ^(const GURL& productURL,
            const std::optional<const commerce::ProductInfo>& productInfo) {
            PriceNotificationsPriceTrackingMediator* strongSelf = weakSelf;
            if (!strongSelf) {
              return;
            }

            strongSelf.currentSiteProductInfo = productInfo;
            [strongSelf fetchTrackableItemDataAtSite:currentSiteURL];
            [strongSelf fetchTrackedItems];
          }));
}

// Creates a `PriceNotificationsTableViewItem` object and sends the newly
// created object to the Price Notifications UI.
- (void)addTrackedItem:(const std::optional<commerce::ProductInfo>&)productInfo
              fromSite:(const GURL&)URL {
  if (!productInfo) {
    return;
  }

  PriceNotificationsTableViewItem* item =
      [self createPriceNotificationTableViewItem:YES
                                 fromProductInfo:productInfo
                                           atURL:URL];
  [self.consumer
      addTrackedItem:item
         toBeginning:[self isCurrentSiteEqualToProductInfo:productInfo]];

  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;
  // Fetches the current item's trackable image.
  _imageFetcher->FetchImageData(
      productInfo->image_url,
      base::BindOnce(^(const std::string& imageData,
                       const image_fetcher::RequestMetadata& metadata) {
        [weakSelf updateItem:item withImage:imageData];
      }),
      NO_TRAFFIC_ANNOTATION_YET);
}

// Creates and initializes the values of a new PriceNotificationTableViewItem
// based on the given `productInfo` object.
- (PriceNotificationsTableViewItem*)
    createPriceNotificationTableViewItem:(BOOL)forTrackedItem
                         fromProductInfo:
                             (const std::optional<commerce::ProductInfo>&)
                                 productInfo
                                   atURL:(const GURL&)URL {
  PriceNotificationsTableViewItem* item =
      [[PriceNotificationsTableViewItem alloc] initWithType:0];
  item.title = base::SysUTF8ToNSString(productInfo->title);
  item.entryURL = URL;
  item.tracking = forTrackedItem;
  item.currentPrice = [self extractFormattedCurrentPrice:YES
                                         fromProductInfo:productInfo];
  item.previousPrice = [self extractFormattedCurrentPrice:NO
                                          fromProductInfo:productInfo];
  if (!item.previousPrice) {
    item.previousPrice = item.currentPrice;
    item.currentPrice = nil;
  }

  return item;
}

// Compares two commerce::ProductInfo objects for equality based on the
// `product_cluster_id` property.
- (BOOL)isCurrentSiteEqualToProductInfo:
    (const std::optional<commerce::ProductInfo>&)productInfo {
  if (!productInfo || !productInfo->product_cluster_id.has_value() ||
      !self.currentSiteProductInfo ||
      !self.currentSiteProductInfo->product_cluster_id.has_value()) {
    return false;
  }

  return productInfo->product_cluster_id.value() ==
         self.currentSiteProductInfo->product_cluster_id.value();
}

// Checks if the item being offered at `URL` is already
// bookmarked and being price tracked.
- (BOOL)isPriceTrackingURL:(const GURL&)URL {
  if (!self.bookmarkModel->IsBookmarked(URL)) {
    return false;
  }

  for (const bookmarks::BookmarkNode* node :
       self.bookmarkModel->GetNodesByURL(URL)) {
    std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
        power_bookmarks::GetNodePowerBookmarkMeta(self.bookmarkModel, node);

    if (!meta || !meta->has_shopping_specifics() ||
        !meta->shopping_specifics().has_product_cluster_id()) {
      continue;
    }

    // TODO: b/355423868 - This should use the async version of IsSubscribed.
    if (self.shoppingService->IsSubscribedFromCache(
            commerce::BuildUserSubscriptionForClusterId(
                meta->shopping_specifics().product_cluster_id()))) {
      return true;
    }
  }

  return false;
}

- (void)recordProductStatusFromSource:(PriceNotificationTrackingSource)source
                               status:(PriceNotificationProductStatus)status {
  switch (source) {
    case PriceNotificationTrackingSource::kPriceTracking:
      base::UmaHistogramEnumeration(kPriceTrackingStatusHistogram, status);
      break;
    case PriceNotificationTrackingSource::kPriceInsights:
      base::UmaHistogramEnumeration(kPriceInsightsTrackingStatusHistogram,
                                    status);
      break;
  }
}

- (void)navigateToWebpageForURL:(const GURL&)URL
                    disposition:(WindowOpenDisposition)disposition {
  self.webState->OpenURL(web::WebState::OpenURLParams(
      URL, web::Referrer(), disposition, ui::PAGE_TRANSITION_GENERATED,
      /*is_renderer_initiated=*/false));
}

- (void)stopTrackingForURL:(const GURL&)URL
     withCompletionHandler:(void (^)(BOOL success))completionHandler {
  // Retrieve the bookmark node for the given URL.
  const bookmarks::BookmarkNode* bookmark =
      self.bookmarkModel->GetMostRecentlyAddedUserNodeForURL(URL);

  if (!bookmark) {
    return;
  }

  commerce::SetPriceTrackingStateForBookmark(
      self.shoppingService, self.bookmarkModel, bookmark, false,
      base::BindOnce(completionHandler));
}

// Stops tracking a product's price by URL or cluster ID.
- (void)stopTrackingForURL:(const GURL&)URL
                 clusterId:(uint64_t)clusterId
     withCompletionHandler:(void (^)(BOOL success))completionHandler {
  // Retrieve the bookmark node for the given URL.
  const bookmarks::BookmarkNode* bookmark =
      self.bookmarkModel->GetMostRecentlyAddedUserNodeForURL(URL);

  if (!bookmark) {
    // If the URL isn't bookmarked, try to stop tracking for the given cluster
    // ID.
    commerce::SetPriceTrackingStateForClusterId(
        self.shoppingService, self.bookmarkModel, clusterId, false,
        base::BindOnce(completionHandler));
    return;
  }

  commerce::SetPriceTrackingStateForBookmark(
      self.shoppingService, self.bookmarkModel, bookmark, false,
      base::BindOnce(completionHandler));
}

- (void)presentNotificationPermission:
    (void (^)(BOOL granted, BOOL promptShown, NSError* error))
        completionHandler {
  // Requests push notification permission. This will determine whether the user
  // receives price tracking notifications to the current device. However, the
  // device's permission status will not prevent the shopping service from
  // subscribing the user to the product and its price tracking events.
  [PushNotificationUtil requestPushNotificationPermission:completionHandler];
}

- (void)trackForURL:(const GURL&)URL
                title:(NSString*)title
    completionHandler:(void (^)(BOOL success))completionHandler {
  // The price tracking infrastructure is built on top of bookmarks, so a new
  // bookmark needs to be created before the item can be registered for price
  // tracking.
  const bookmarks::BookmarkNode* bookmark =
      self.bookmarkModel->GetMostRecentlyAddedUserNodeForURL(URL);
  bool isNewBookmark = bookmark == nullptr;
  if (!bookmark) {
    const bookmarks::BookmarkNode* defaultFolder =
        self.bookmarkModel->account_mobile_node();
    if (!defaultFolder) {
      // Cannot track URL: the user is likely signed out.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(completionHandler, false));
      return;
    }
    bookmark = self.bookmarkModel->AddURL(defaultFolder,
                                          defaultFolder->children().size(),
                                          base::SysNSStringToUTF16(title), URL);
  }

  commerce::SetPriceTrackingStateForBookmark(
      self.shoppingService, self.bookmarkModel, bookmark, true,
      base::BindOnce(completionHandler), isNewBookmark);
}

// Callback invoked after requesting push notification permission.
- (void)onNotificationPermissionRequestForItem:(PriceInsightsItem*)item
                             permissionGranted:(BOOL)granted
                                   promptShown:(BOOL)promptShown
                                         error:(NSError*)error {
  if (error) {
    [self priceInsightsTrackItem:item
            notificationsGranted:false
                  showCompletion:true];
    return;
  }

  if (!promptShown && !granted) {
    [self.priceInsightsConsumer presentPushNotificationPermissionAlert];
    return;
  }

  if (promptShown && granted) {
    self.pushNotificationService->SetPreference(
        self.gaiaID, PushNotificationClientId::kCommerce, true);
  }

  [self priceInsightsTrackItem:item
          notificationsGranted:granted
                showCompletion:true];
}

// Callback invoked after requesting to track an item.
- (void)onPriceInsightsTrackItem:(PriceInsightsItem*)item
                         success:(BOOL)success
               permissionGranted:(BOOL)granted
                  showCompletion:(BOOL)showCompletion {
  if (!success) {
    [self.priceInsightsConsumer presentStartPriceTrackingErrorSnackbar];
    return;
  }

  [self.priceInsightsConsumer
      didStartPriceTrackingWithNotification:granted
                             showCompletion:showCompletion];

  [self recordProductStatusFromSource:PriceNotificationTrackingSource::
                                          kPriceInsights
                               status:PriceNotificationProductStatus::kTrack];
}

// Callback invoked after requesting to stop tracking an item.
- (void)onPriceInsightsStopTrackingItem:(PriceInsightsItem*)item
                                success:(BOOL)success {
  if (!success) {
    [self.priceInsightsConsumer presentStopPriceTrackingErrorSnackbar];
    return;
  }

  [self recordProductStatusFromSource:PriceNotificationTrackingSource::
                                          kPriceInsights
                               status:PriceNotificationProductStatus::kUntrack];
  [self.priceInsightsConsumer didStopPriceTracking];
}

@end
