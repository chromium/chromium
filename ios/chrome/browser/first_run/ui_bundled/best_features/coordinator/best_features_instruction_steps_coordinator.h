// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_COORDINATOR_BEST_FEATURES_INSTRUCTION_STEPS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_COORDINATOR_BEST_FEATURES_INSTRUCTION_STEPS_COORDINATOR_H_

#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_instruction_steps_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class BestFeaturesItem;

// Coordinator for the Best Features instruction steps.
@interface BestFeaturesInstructionStepsCoordinator : ChromeCoordinator

// Initializes the coordinator.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      item:(BestFeaturesItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_COORDINATOR_BEST_FEATURES_INSTRUCTION_STEPS_COORDINATOR_H_
