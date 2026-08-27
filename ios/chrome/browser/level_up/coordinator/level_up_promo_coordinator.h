// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol LevelUpPromoCoordinatorDelegate;

// Coordinator in charge of the Level Up Promo Sheet.
@interface LevelUpPromoCoordinator : ChromeCoordinator

// The delegate object to receive user decisions from the promo sheet.
@property(nonatomic, weak) id<LevelUpPromoCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_PROMO_COORDINATOR_H_
