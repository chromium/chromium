// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_FIRST_RUN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_FIRST_RUN_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator that manages the first run and any BWG triggers.
@interface GeminiFirstRunCoordinator : ChromeCoordinator

// Initializes the coordinator with a specific First Run type. `entryPoint`
// denotes where the flow starts from, and `completion` is called when the flow
// finishes, with `success` indicating whether the First Run was completed.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            fromEntryPoint:(gemini::EntryPoint)entryPoint
                              firstRunType:(GeminiFirstRunType)firstRunType
                         completionHandler:(void (^)(BOOL success))completion
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Dismisses the BWG flow with a completion block before stopping the
// coordinator.
- (void)stopWithCompletion:(ProceduralBlock)completion;

// Whether the presentation of the FRE view controller should be animated.
@property(nonatomic, assign) BOOL animatedPresentation;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_FIRST_RUN_COORDINATOR_H_
