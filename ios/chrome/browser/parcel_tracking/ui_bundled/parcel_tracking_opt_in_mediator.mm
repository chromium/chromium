// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/parcel_tracking/ui_bundled/parcel_tracking_opt_in_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/commerce/core/shopping_service.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_step.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/parcel_tracking/tracking_source.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation ParcelTrackingOptInMediator {
  raw_ptr<commerce::ShoppingService> _shoppingService;
}

- (instancetype)initWithShoppingService:
    (commerce::ShoppingService*)shoppingService {
  CHECK(shoppingService);

  self = [super init];
  if (self) {
    _shoppingService = shoppingService;
  }
  return self;
}

- (void)didTapAlwaysTrack:(NSArray<CustomTextCheckingResult*>*)parcelList {
  TrackParcels(_shoppingService, parcelList, std::string(),
               _parcelTrackingCommandsHandler, true,
               TrackingSource::kAutoTrack);
}

- (void)didTapAskToTrack:(NSArray<CustomTextCheckingResult*>*)parcelList {
  [_parcelTrackingCommandsHandler
      showParcelTrackingInfobarWithParcels:parcelList
                                   forStep:ParcelTrackingStep::
                                               kAskedToTrackPackage];
}

@end
