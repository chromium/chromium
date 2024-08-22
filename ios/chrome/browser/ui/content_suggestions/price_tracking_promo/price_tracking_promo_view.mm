// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_view.h"

@implementation PriceTrackingPromoModuleView {
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:CGRectZero];
  return self;
}

- (void)configureView:(PriceTrackingPromoItem*)config {
  // TODO(crbug.com/361106621) implement magic stack card
  // (latest subscription item image, information on how
  // to enable price tracking on Chrome for iOS).
}

@end
