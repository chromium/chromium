// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_GUIDED_TOUR_COORDINATOR_GUIDED_TOUR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_GUIDED_TOUR_COORDINATOR_GUIDED_TOUR_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"



// Coordinator to present a Guided Tour step.
@interface GuidedTourCoordinator : ChromeCoordinator

// Initializes a GuidedTourCoordinator with `baseViewController`,
// `browser`, and `completionBlock`.
- (instancetype)initWithStep:(GuidedTourStep)step
          baseViewController:(UIViewController*)baseViewController
                     browser:(Browser*)browser
             completionBlock:(ProceduralBlock)completionBlock
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_GUIDED_TOUR_COORDINATOR_GUIDED_TOUR_COORDINATOR_H_
