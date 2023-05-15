// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_HALF_SCREEN_PROMO_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_HALF_SCREEN_PROMO_COORDINATOR_DELEGATE_H_

#import "ios/chrome/browser/ui/default_promo/half_screen_promo_coordinator.h"

// Delegate protocol to handle communication from HalfScreenPromoCoordinator to
// the parent coordinator.
@protocol HalfScreenPromoCoordinatorDelegate

// Invoked when the user clicks on the primary button.
- (void)handlePrimaryActionForHalfScreenPromoCoordinator:
    (HalfScreenPromoCoordinator*)coordinator;

// Invoked when the user clicks on the secondary button.
- (void)handleSecondaryActionForHalfScreenPromoCoordinator:
    (HalfScreenPromoCoordinator*)coordinator;

// Invoked when the user dismiss the promo.
- (void)handleDismissActionForHalfScreenPromoCoordinator:
    (HalfScreenPromoCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_HALF_SCREEN_PROMO_COORDINATOR_DELEGATE_H_
