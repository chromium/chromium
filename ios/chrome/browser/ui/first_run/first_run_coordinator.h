// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class FirstRunScreenProvider;

// The delegate for the FirstRunCoordinator.
@protocol FirstRunCoordinatorDelegate <NSObject>

// Called when first run screens should finish presenting.
- (void)willFinishPresentingScreens;

// Called when first run UI has been dismissed, with |continuedAction|
- (void)didFinishPresentingScreensWithSubsequentActionsTriggered:
    (BOOL)actionsTriggered;

@end

// Coordinator to present first run screens.
@interface FirstRunCoordinator : ChromeCoordinator

// Initiate the coordinator.|screenProvider| will help decide which screen to
// show.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            screenProvider:
                                (FirstRunScreenProvider*)screenProvider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(nonatomic, weak) id<FirstRunCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_COORDINATOR_H_
