// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_PROMO_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Delegate for GuidedTourPromoCoordinator to handle user actions.
@protocol GuidedTourPromoCoordinatorDelegate

// Indicates to the delegate to start the guided tour.
- (void)startGuidedTour;

// Indicates to the delegate to dismiss the promo.
- (void)dismissGuidedTourPromo;

@end

// Coordinator to present the Guided Tour Promo.
@interface GuidedTourPromoCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<GuidedTourPromoCoordinatorDelegate> delegate;

// Designated stop method with `completion` executed after the promo is
// dismissed.
- (void)stopWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_PROMO_COORDINATOR_H_
