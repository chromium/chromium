// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_mediator.h"

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
  // TODO(crbug.com/1473449): Call on AnnotationsTabHelper to track parcels once
  // Shopping Service API is ready.
  // TODO(crbug.com/1473449): trigger infobar.
}

- (void)didTapTertiaryActionButton:
    (NSArray<CustomTextCheckingResult*>*)parcelList {
  // TODO(crbug.com/1473449): trigger infobar.
}

@end
