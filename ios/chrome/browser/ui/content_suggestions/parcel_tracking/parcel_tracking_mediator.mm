// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_mediator.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/shopping_service.h"
#import "ios/chrome/browser/parcel_tracking/metrics.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/parcel_tracking/tracking_source.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_item.h"
#import "ios/chrome/browser/ui/ntp/metrics/home_metrics.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

@interface ParcelTrackingMediator ()
@end

@implementation ParcelTrackingMediator {
  raw_ptr<commerce::ShoppingService> _shoppingService;
  NSArray<ParcelTrackingItem*>* _parcelTrackingItems;
  UrlLoadingBrowserAgent* _URLLoadingBrowserAgent;
}

- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)shoppingService
     URLLoadingBrowserAgent:(UrlLoadingBrowserAgent*)URLLoadingBrowserAgent {
  self = [super init];
  if (self) {
    _shoppingService = shoppingService;
    _URLLoadingBrowserAgent = URLLoadingBrowserAgent;
  }
  return self;
}

- (void)disconnect {
  _shoppingService = nil;
  _URLLoadingBrowserAgent = nil;
  _delegate = nil;
}

#pragma mark - Public

- (ParcelTrackingItem*)parcelTrackingItemToShow {
  if ([_parcelTrackingItems count] > 1) {
    ParcelTrackingItem* itemToShow = _parcelTrackingItems[0];
    itemToShow.shouldShowSeeMore = YES;
    return itemToShow;
  }
  return _parcelTrackingItems[0];
}

- (NSArray<ParcelTrackingItem*>*)allParcelTrackingItems {
  return _parcelTrackingItems;
}

- (void)disableModule {
  DisableParcelTracking(GetApplicationContext()->GetLocalState());
  _shoppingService->StopTrackingAllParcels(base::BindOnce(^(bool){
  }));

  [self.delegate parcelTrackingDisabled];
}

- (void)untrackParcel:(NSString*)parcelID {
  _shoppingService->StopTrackingParcel(
      base::SysNSStringToUTF8(parcelID), base::BindOnce(^(bool) {
        parcel_tracking::RecordParcelsUntracked(
            TrackingSource::kMagicStackModule, 1);
      }));
}

- (void)trackParcel:(NSString*)parcelID carrier:(ParcelType)carrier {
  commerce::ParcelIdentifier::Carrier carrierValue =
      [self carrierValueForParcelType:carrier];
  _shoppingService->StartTrackingParcels(
      {std::make_pair(carrierValue, base::SysNSStringToUTF8(parcelID))},
      std::string(),
      base::BindOnce(
          ^(bool, std::unique_ptr<std::vector<commerce::ParcelTrackingStatus>>){
          }));
}

- (void)setDelegate:(id<ParcelTrackingMediatorDelegate>)delegate {
  if (delegate == _delegate) {
    return;
  }
  _delegate = delegate;
  if (_delegate) {
    __weak ParcelTrackingMediator* weakSelf = self;
    _shoppingService->GetAllParcelStatuses(base::BindOnce(^(
        bool success,
        std::unique_ptr<std::vector<commerce::ParcelTrackingStatus>> parcels) {
      ParcelTrackingMediator* strongSelf = weakSelf;
      if (!strongSelf || !success || !strongSelf.delegate) {
        return;
      }
      [strongSelf parcelStatusesSuccessfullyReceived:std::move(parcels)];
    }));
  }
}

#pragma mark - ParcelTrackingCommands

- (void)loadParcelTrackingPage:(GURL)parcelTrackingURL {
  [self.NTPMetricsDelegate parcelTrackingOpened];
  [self.delegate logMagicStackEngagementForType:ContentSuggestionsModuleType::
                                                    kParcelTracking];
  _URLLoadingBrowserAgent->Load(UrlLoadParams::InCurrentTab(parcelTrackingURL));
}

#pragma mark - Private

// Handles a parcel tracking status fetch result from the
// commerce::ShoppingService.
- (void)parcelStatusesSuccessfullyReceived:
    (std::unique_ptr<std::vector<commerce::ParcelTrackingStatus>>)
        parcelStatuses {
  NSMutableArray* parcelItems = [NSMutableArray array];

  for (auto iter = parcelStatuses->begin(); iter != parcelStatuses->end();
       ++iter) {
    ParcelTrackingItem* item = [[ParcelTrackingItem alloc] init];
    item.parcelType = [self parcelTypeforCarrierValue:iter->carrier];
    item.estimatedDeliveryTime = iter->estimated_delivery_time;
    item.parcelID = base::SysUTF8ToNSString(iter->tracking_id);
    item.trackingURL = iter->tracking_url;
    item.status = (ParcelState)iter->state;
    item.commandHandler = self;
    [parcelItems addObject:item];

    if (!iter->estimated_delivery_time.is_null() &&
        iter->estimated_delivery_time < base::Time::Now() - base::Days(2)) {
      // Parcel was delivered more than two days ago, make this the last time it
      // is shown by stopping tracking.
      _shoppingService->StopTrackingParcel(iter->tracking_id,
                                           base::BindOnce(^(bool){
                                           }));
    }
  }

  if ([parcelItems count] > 0) {
    _parcelTrackingItems = parcelItems;
    [self logParcelTrackingFreshnessSignalIfApplicable];
    [self.delegate newParcelsAvailable];
  }
}

// Logs a freshness signal for the Parcel Tracking module if there is at least
// one parcel with an estimated delivery date within the next two days.
- (void)logParcelTrackingFreshnessSignalIfApplicable {
  for (ParcelTrackingItem* item in _parcelTrackingItems) {
    base::Time now = base::Time::Now();
    if (item.estimatedDeliveryTime > now &&
        item.estimatedDeliveryTime < now + base::Days(2)) {
      RecordModuleFreshnessSignal(
          ContentSuggestionsModuleType::kParcelTracking);
      return;
    }
  }
}

// Maps the carrier int value into a ParcelType.
- (ParcelType)parcelTypeforCarrierValue:(int)carrier {
  if (carrier == 1) {
    return ParcelType::kFedex;
  } else if (carrier == 2) {
    return ParcelType::kUPS;
  } else if (carrier == 4) {
    return ParcelType::kUSPS;
  }
  return ParcelType::kUnkown;
}

- (commerce::ParcelIdentifier::Carrier)carrierValueForParcelType:
    (ParcelType)parcelType {
  switch (parcelType) {
    case ParcelType::kUSPS:
      return commerce::ParcelIdentifier::Carrier(4);
    case ParcelType::kUPS:
      return commerce::ParcelIdentifier::Carrier(2);
    case ParcelType::kFedex:
      return commerce::ParcelIdentifier::Carrier(1);
    default:
      return commerce::ParcelIdentifier::Carrier(0);
  }
}

@end
