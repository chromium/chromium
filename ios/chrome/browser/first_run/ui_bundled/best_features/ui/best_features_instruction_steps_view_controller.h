// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_INSTRUCTION_STEPS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_INSTRUCTION_STEPS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/tips_notifications/ui/instructions_bottom_sheet_view_controller.h"

@class BestFeaturesItem;

// A view controller that shows the instruction steps for a Best Feature.
@interface BestFeaturesInstructionStepsViewController
    : InstructionsBottomSheetViewController

// Initializes the view controller with a BestFeaturesItem.
- (instancetype)initWithItem:(BestFeaturesItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_INSTRUCTION_STEPS_VIEW_CONTROLLER_H_
