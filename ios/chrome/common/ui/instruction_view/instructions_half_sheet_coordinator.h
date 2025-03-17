// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_INSTRUCTION_VIEW_INSTRUCTIONS_HALF_SHEET_COORDINATOR_H_
#define IOS_CHROME_COMMON_UI_INSTRUCTION_VIEW_INSTRUCTIONS_HALF_SHEET_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator to present half screen instructions.
@interface InstructionsHalfSheetCoordinator : ChromeCoordinator

// Initializes a Instructions Half Sheet Coordinator with `baseViewController`,
// `browser`, and `instructionsList`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                          instructionsList:(NSArray<NSString*>*)instructionsList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The title text to be passed to the `InstructionsHalfSheetViewController`.
@property(nonatomic, weak) NSString* titleText;

@end

#endif  // IOS_CHROME_COMMON_UI_INSTRUCTION_VIEW_INSTRUCTIONS_HALF_SHEET_COORDINATOR_H_
