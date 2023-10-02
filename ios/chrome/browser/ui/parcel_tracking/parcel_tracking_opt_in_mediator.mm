// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_mediator.h"

#import "components/commerce/core/shopping_service.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_step.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

@implementation ParcelTrackingOptInMediator {
  web::WebState* _webState;
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _webState = webState;
  }
  return self;
}

- (void)didTapPrimaryActionButton:
    (NSArray<CustomTextCheckingResult*>*)parcelList {
  commerce::ShoppingService* shoppingService =
      commerce::ShoppingServiceFactory::GetForBrowserState(
          _webState->GetBrowserState());
  TrackParcels(shoppingService, parcelList, std::string(),
               _parcelTrackingCommandsHandler, true);
}

- (void)didTapTertiaryActionButton:
    (NSArray<CustomTextCheckingResult*>*)parcelList {
  [_parcelTrackingCommandsHandler
      showParcelTrackingInfobarWithParcels:parcelList
                                   forStep:ParcelTrackingStep::
                                               kAskedToTrackPackage];
}

@end
