// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"

// Delegate for GuidedTourCoordinator to handle user actions.
@protocol GuidedTourCoordinatorDelegate

// Indicates to the delegate that the user tapped on the next button for `step`.
- (void)nextTappedForStep:(GuidedTourStep)step;

// Indicates to the delegate that the `step` was dismissed.
- (void)stepCompleted:(GuidedTourStep)step;

@end

// Coordinator to present a Guided Tour step.
@interface GuidedTourCoordinator : ChromeCoordinator

// Initializes a GuidedTourCoordinator with `baseViewController`,
// `browser`, and `delegate`.
- (instancetype)initWithStep:(GuidedTourStep)step
          baseViewController:(UIViewController*)baseViewController
                     browser:(Browser*)browser
                    delegate:(id<GuidedTourCoordinatorDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_GUIDED_TOUR_GUIDED_TOUR_COORDINATOR_H_
