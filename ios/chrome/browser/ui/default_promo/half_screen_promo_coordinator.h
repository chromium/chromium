// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_HALF_SCREEN_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_HALF_SCREEN_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol HalfScreenPromoCoordinatorDelegate;

// Coordinator to present the half screen promo
@interface HalfScreenPromoCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<HalfScreenPromoCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_HALF_SCREEN_PROMO_COORDINATOR_H_
