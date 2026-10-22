// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_GUIDED_TOUR_COORDINATOR_GUIDED_TOUR_COORDINATOR_TESTING_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_GUIDED_TOUR_COORDINATOR_GUIDED_TOUR_COORDINATOR_TESTING_H_

#import "ios/chrome/browser/first_run/guided_tour/coordinator/guided_tour_coordinator.h"

@interface GuidedTourCoordinator (Testing)

// Dismisses the Guided Tour promo when rejected by the user.
- (void)dismissGuidedTourPromo;

// Starts the Guided Tour steps after the user accepts the promo.
- (void)startGuidedTour;

// Handles the completion of the NTP step of the Guided Tour.
- (void)guidedTourNTPStepCompleted;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_GUIDED_TOUR_COORDINATOR_GUIDED_TOUR_COORDINATOR_TESTING_H_
