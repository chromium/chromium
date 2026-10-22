// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_GUIDED_TOUR_COORDINATOR_GUIDED_TOUR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_GUIDED_TOUR_COORDINATOR_GUIDED_TOUR_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator to present the Guided Tour Promo and drive the Guided Tour steps.
@interface GuidedTourCoordinator : ChromeCoordinator

// Completion block called when the Guided Tour flow finishes, indicating
// whether the user started the tour.
@property(nonatomic, copy) void (^completion)(BOOL guidedTourStarted);

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_GUIDED_TOUR_COORDINATOR_GUIDED_TOUR_COORDINATOR_H_
