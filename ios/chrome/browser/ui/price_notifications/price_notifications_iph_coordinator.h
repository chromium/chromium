// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_IPH_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_IPH_COORDINATOR_H_

#import "ios/chrome/browser/commerce/price_notifications/price_notifications_iph_presenter.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// Coordinator for the Follow IPH feature.
@interface PriceNotificationsIPHCoordinator
    : ChromeCoordinator <PriceNotificationsIPHPresenter>
@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_IPH_COORDINATOR_H_
