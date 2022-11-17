// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_price_tracking_mediator.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/payments/core/currency_formatter.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"
#import "ios/web/public/web_state.h"

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
      [[PriceNotificationsTableViewItem alloc] initWithType:0];
  item.title = base::SysUTF8ToNSString(productInfo->title);
  item.entryURL = base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              URL));
  item.currentPrice = nil;
  item.previousPrice = [self formatPrice:productInfo];
  item.tracking = NO;

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
- (NSString*)formatPrice:
    (const absl::optional<commerce::ProductInfo>&)productInfo {
  if (!productInfo) {
    return nil;
  }

  float price = productInfo->amount_micros / commerce::kToMicroCurrency;
  std::unique_ptr<payments::CurrencyFormatter> formatter =
      std::make_unique<payments::CurrencyFormatter>(productInfo->currency_code,
                                                    productInfo->country_code);
  formatter->SetMaxFractionalDigits(2);
  return base::SysUTF16ToNSString(
      formatter->Format(base::NumberToString(price)));
}

@end
