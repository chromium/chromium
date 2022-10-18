// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_primary_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PriceNotificationsPrimaryMediator ()
// The service responsible for obtaining the product's pricing information and
// metadata.
@property(nonatomic, assign) commerce::ShoppingService* shoppingService;

@end

@implementation PriceNotificationsPrimaryMediator

- (instancetype)initWithShoppingService:(commerce::ShoppingService*)service {
  self = [super init];
  if (self) {
    DCHECK(service);
    _shoppingService = service;
  }

  return self;
}
@end
