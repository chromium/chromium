// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_PROMO_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_PROMO_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class LevelUpPromoCoordinator;

// Delegate protocol to handle communication from LevelUpPromoCoordinator
// to the parent LevelUpCoordinator.
@protocol LevelUpPromoCoordinatorDelegate <NSObject>

// Notifies the delegate that the user opted in from the promo screen.
- (void)levelUpPromoCoordinatorDidOptIn:(LevelUpPromoCoordinator*)coordinator;

// Notifies the delegate that the user dismissed/cancelled the promo screen.
- (void)levelUpPromoCoordinatorDidCancel:(LevelUpPromoCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_PROMO_COORDINATOR_DELEGATE_H_
