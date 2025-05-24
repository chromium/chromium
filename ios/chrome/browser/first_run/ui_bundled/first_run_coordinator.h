// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ScreenProvider;

// The delegate for the FirstRunCoordinator.
@protocol FirstRunCoordinatorDelegate <NSObject>

// Called when first run is done and the coordinator can be stopped.
- (void)didFinishFirstRun;

@end

// Coordinator to present first run screens.
@interface FirstRunCoordinator : ChromeCoordinator

// Initiate the coordinator.
// `browser` used for authentication. It must not be off the record (incognito).
// `screenProvider` helps decide which screen to show.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            screenProvider:(ScreenProvider*)screenProvider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated stop method for this coordinator with `completionHandler` executed
// after its UI finishes dismissing.
- (void)stopWithCompletion:(ProceduralBlock)completionHandler;

@property(nonatomic, weak) id<FirstRunCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_COORDINATOR_H_
