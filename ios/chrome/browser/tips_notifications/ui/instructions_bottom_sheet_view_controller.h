// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_UI_INSTRUCTIONS_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_UI_INSTRUCTIONS_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/bottom_sheet/bottom_sheet_view_controller.h"

// A view controller that shows a set of instructions in a bottom sheet.
@interface InstructionsBottomSheetViewController : BottomSheetViewController

// An array containing the strings of each step to display in the instructions.
@property(nonatomic, strong) NSArray<NSString*>* steps;

@end

#endif  // IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_UI_INSTRUCTIONS_BOTTOM_SHEET_VIEW_CONTROLLER_H_
