// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_price_tracking_mediator.h"

#import "base/logging.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/payments/core/currency_formatter.h"
#import "ios/chrome/browser/push_notification/push_notification_util.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_consumer.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;
// The current browser state's webstate.
@property(nonatomic, assign) web::WebState* webState;

@end

@implementation PriceNotificationsPriceTrackingMediator

- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)service
              bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
               imageFetcher:
                   (std::unique_ptr<image_fetcher::ImageDataFetcher>)fetcher
                   webState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    DCHECK(service);
    DCHECK(bookmarkModel);
    DCHECK(fetcher);
    DCHECK(webState);
    _shoppingService = service;
    _bookmarkModel = bookmarkModel;
    _imageFetcher = std::move(fetcher);
    _webState = webState;
  }

  return self;
}

- (void)setConsumer:(id<PriceNotificationsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [self fetchTrackableItemDataAtSite:self.webState->GetVisibleURL()];
  [self fetchTrackedItems];
}

#pragma mark - PriceNotificationsMutator

- (void)trackItem:(PriceNotificationsTableViewItem*)item {
  // Requests push notification permission. This will determine whether the user
  // receives price tracking notifications to the current device. However, the
  // device's permission status will not prevent the shopping service from
  // subscribing the user to the product and its price tracking events.
  [PushNotificationUtil requestPushNotificationPermission:nil];

  // The price tracking infrastructure is built on top of bookmarks, so a new
  // bookmark needs to be created before the item can be registered for price
  // tracking.
  const bookmarks::BookmarkNode* bookmark =
      self.bookmarkModel->GetMostRecentlyAddedUserNodeForURL(item.entryURL);
  if (!bookmark) {
    const bookmarks::BookmarkNode* defaultFolder =
        self.bookmarkModel->mobile_node();
    bookmark = self.bookmarkModel->AddURL(
        defaultFolder, defaultFolder->children().size(),
        base::SysNSStringToUTF16(item.title), item.entryURL);
  }

  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;
  commerce::SetPriceTrackingStateForBookmark(
      self.shoppingService, self.bookmarkModel, bookmark, true,
      base::BindOnce(^(bool success) {
        [weakSelf didTrackItem:item successfully:success];
      }));
}

- (void)stopTrackingItem:(PriceNotificationsTableViewItem*)item {
  // Retrieve the bookmark node for the given URL.
  const bookmarks::BookmarkNode* bookmark =
      self.bookmarkModel->GetMostRecentlyAddedUserNodeForURL(item.entryURL);

  if (!bookmark) {
    return;
  }

  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;
  commerce::SetPriceTrackingStateForBookmark(
      self.shoppingService, self.bookmarkModel, bookmark, false,
      base::BindOnce(^(bool success) {
        if (!success) {
          return;
        }
        [weakSelf didStopTrackingItem:item];
      }));
}

#pragma mark - Private

// This function fetches the product data for the item on the currently visible
// page and populates the data into the Price Notifications UI.
- (void)fetchTrackableItemDataAtSite:(const GURL&)URL {
  // Checks if the item being offered on the current site is already being
  // tracked.
  if (self.bookmarkModel->IsBookmarked(URL)) {
    std::vector<const bookmarks::BookmarkNode*> nodes;
    self.bookmarkModel->GetNodesByURL(URL, &nodes);
    const bookmarks::BookmarkNode* node = nodes[0];
    if (commerce::IsBookmarkPriceTracked(self.bookmarkModel, node)) {
      [self.consumer setTrackableItem:nil currentlyTracking:YES];
      return;
    }
  }

  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;

  self.shoppingService->GetProductInfoForUrl(
      URL, base::BindOnce(
               ^(const GURL& productURL,
                 const absl::optional<commerce::ProductInfo>& productInfo) {
                 [weakSelf displayProduct:productInfo fromSite:productURL];
               }));
}

// Creates a `PriceNotificationsTableViewItem` object and sends the newly
// created object to the Price Notifications UI.
- (void)displayProduct:(const absl::optional<commerce::ProductInfo>&)productInfo
              fromSite:(const GURL&)URL {
  if (!productInfo) {
    [self.consumer setTrackableItem:nil currentlyTracking:NO];
    return;
  }

  PriceNotificationsTableViewItem* item =
      [self createPriceNotificationTableViewItem:NO
                                 fromProductInfo:productInfo
                                           atURL:URL];
  [self.consumer setTrackableItem:item currentlyTracking:NO];

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
                              (const absl::optional<commerce::ProductInfo>&)
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
  if (success) {
    trackableItem.tracking = YES;
    [self.consumer reconfigureCellsForItems:@[ trackableItem ]];
    [self.consumer didStartPriceTrackingForItem:trackableItem];
  }

  // TODO(crbug.com/1400738) Implement UX flow in the event an error occurs when
  // a user attempts to track an item.
}

// This function handles the response from the user attempting to unsubscribe to
// an item with the ShoppingService.
- (void)didStopTrackingItem:(PriceNotificationsTableViewItem*)item {
  [self.consumer
      didStopPriceTrackingItem:item
                 onCurrentSite:self.webState->GetVisibleURL() == item.entryURL];
}

// This function fetches the product data for the items the user has subscribed
// to and populates the data into the Price Notifications UI.
- (void)fetchTrackedItems {
  std::vector<const bookmarks::BookmarkNode*> subscribedItems =
      commerce::GetAllPriceTrackedBookmarks(self.bookmarkModel);
  std::vector<int64_t> bookmarkIDs;
  for (const bookmarks::BookmarkNode* bookmark : subscribedItems) {
    bookmarkIDs.push_back(bookmark->id());
  }

  __weak PriceNotificationsPriceTrackingMediator* weakSelf = self;
  self.shoppingService->GetUpdatedProductInfoForBookmarks(
      bookmarkIDs,
      base::BindRepeating(^(const int64_t bookmarkID, const GURL& productURL,
                            absl::optional<commerce::ProductInfo> productInfo) {
        [weakSelf addTrackedItem:productInfo fromSite:productURL];
      }));
}

// Creates a `PriceNotificationsTableViewItem` object and sends the newly
// created object to the Price Notifications UI.
- (void)addTrackedItem:(const absl::optional<commerce::ProductInfo>&)productInfo
              fromSite:(const GURL&)URL {
  if (!productInfo) {
    return;
  }

  PriceNotificationsTableViewItem* item =
      [self createPriceNotificationTableViewItem:YES
                                 fromProductInfo:productInfo
                                           atURL:URL];
  [self.consumer addTrackedItem:item];

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
                             (const absl::optional<commerce::ProductInfo>&)
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

@end
