// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_PRIMARY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_PRIMARY_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/commerce/core/shopping_service.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_consumer.h"

@interface PriceNotificationsPrimaryMediator
    : NSObject <PriceNotificationsDataSource>

// The designated initializer. `ShoppingService` must not be nil.
- (instancetype)initWithShoppingService:(commerce::ShoppingService*)service
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<PriceNotificationsConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_PRIMARY_MEDIATOR_H_
