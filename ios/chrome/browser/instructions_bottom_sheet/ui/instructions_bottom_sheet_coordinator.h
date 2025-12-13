// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INSTRUCTIONS_BOTTOM_SHEET_UI_INSTRUCTIONS_BOTTOM_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INSTRUCTIONS_BOTTOM_SHEET_UI_INSTRUCTIONS_BOTTOM_SHEET_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for the instructions bottom sheet.
@interface InstructionsBottomSheetCoordinator : ChromeCoordinator

// Initializes the coordinator.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                     title:(NSString*)title
                                     steps:(NSArray<NSString*>*)steps;

@end

#endif  // IOS_CHROME_BROWSER_INSTRUCTIONS_BOTTOM_SHEET_UI_INSTRUCTIONS_BOTTOM_SHEET_COORDINATOR_H_
