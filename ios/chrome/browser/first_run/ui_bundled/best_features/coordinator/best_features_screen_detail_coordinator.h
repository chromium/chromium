// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_COORDINATOR_BEST_FEATURES_SCREEN_DETAIL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_COORDINATOR_BEST_FEATURES_SCREEN_DETAIL_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol FirstRunScreenDelegate;

@class BestFeaturesItem;

// Coordinator to present the feature specific Best Features Detail Screen.
@interface BestFeaturesScreenDetailCoordinator : ChromeCoordinator

// Delegate to handle events from the Best Features Detail Screen.
@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;

// Initializes a BestFeaturesScreenDetailCoordinator with
// `navigationController`, `browser`, and `BestFeaturesItem`.
- (instancetype)initWithBaseNavigationViewController:
                    (UINavigationController*)navigationController
                                             browser:(Browser*)browser
                                    bestFeaturesItem:
                                        (BestFeaturesItem*)bestFeaturesItem
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_COORDINATOR_BEST_FEATURES_SCREEN_DETAIL_COORDINATOR_H_
