// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_coordinator.h"

@implementation ParcelTrackingOptInCoordinator {
  web::WebState* _webState;
  NSArray<CustomTextCheckingResult*>* _parcels;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState
                                   parcels:(NSArray<CustomTextCheckingResult*>*)
                                               parcels {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _webState = webState;
    _parcels = parcels;
  }
  return self;
}

// TODO(crbug.com/1473449): override start and stop methods.

@end
