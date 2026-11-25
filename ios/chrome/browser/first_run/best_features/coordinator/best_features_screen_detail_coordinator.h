// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_COORDINATOR_BEST_FEATURES_SCREEN_DETAIL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_COORDINATOR_BEST_FEATURES_SCREEN_DETAIL_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol FirstRunScreenDelegate;

@class BestFeaturesItem;

// An enum representing the source that started this coordinator.
enum class DetailScreenPresentationSource {
  kBestOfAppFRE,
  kBestFeaturesFRE,
};

// Coordinator to present the feature specific Best Features Detail Screen.
@interface BestFeaturesScreenDetailCoordinator : ChromeCoordinator

// Delegate to handle events from the Best Features Detail Screen.
@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;

// Initializes a BestFeaturesScreenDetailCoordinator with
// `navigationController`, `browser`, `BestFeaturesItem`, and `firstRun`.
- (instancetype)
    initWithBaseNavigationViewController:
        (UINavigationController*)navigationController
                                 browser:(Browser*)browser
                        bestFeaturesItem:(BestFeaturesItem*)bestFeaturesItem
                                  source:(DetailScreenPresentationSource)source
                                firstRun:(BOOL)firstRun;

// Initializes a BestFeaturesScreenDetailCoordinator with
// `navigationController`, `browser`, `BestFeaturesItems`, `startIndex`, and
// `firstRun`. This is used to create the animated feature carousel.
- (instancetype)
    initWithBaseNavigationViewController:
        (UINavigationController*)navigationController
                                 browser:(Browser*)browser
                       bestFeaturesItems:
                           (NSArray<BestFeaturesItem*>*)bestFeaturesItems
                              startIndex:(int)startIndex
                                  source:(DetailScreenPresentationSource)source
                                firstRun:(BOOL)firstRun
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_COORDINATOR_BEST_FEATURES_SCREEN_DETAIL_COORDINATOR_H_
