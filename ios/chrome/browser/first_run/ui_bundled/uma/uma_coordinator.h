// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_UMA_UMA_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_UMA_UMA_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class UMACoordinator;

// Delegate for UMACoordinator.
@protocol UMACoordinatorDelegate <NSObject>

// Called when the coordinator has been removed from the screen.
// `userChoice` whether the user accepts UMA reporting.
- (void)UMACoordinatorDidRemoveWithCoordinator:(UMACoordinator*)coordinator
                        UMAReportingUserChoice:(BOOL)UMAReportingUserChoice;

@end

// Coordinator to present UMA dialog in the FRE. The owner of UMACoordinator
// is in charge set metrics::prefs::kMetricsReportingEnabled based on the user
// choice.
@interface UMACoordinator : ChromeCoordinator

@property(nonatomic, weak) id<UMACoordinatorDelegate> delegate;

// Initiates UMACoordinator with `viewController` and `browser`.
// `UMAReportingValue` is the UMA toggle value, when opening the dialog.
// It should be kDefaultMetricsReportingCheckboxValue if the user never opened
// the dialog yet.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                         UMAReportingValue:(BOOL)UMAReportingValue
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_UMA_UMA_COORDINATOR_H_
