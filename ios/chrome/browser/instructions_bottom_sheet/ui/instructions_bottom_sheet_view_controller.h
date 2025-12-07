// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INSTRUCTIONS_BOTTOM_SHEET_UI_INSTRUCTIONS_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INSTRUCTIONS_BOTTOM_SHEET_UI_INSTRUCTIONS_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/bottom_sheet/bottom_sheet_view_controller.h"

// A view controller that shows a set of instructions in a bottom sheet.
@interface InstructionsBottomSheetViewController : BottomSheetViewController

// Initializes the view controller with a title and a list of instructions.
- (instancetype)initWithTitle:(NSString*)title
                 instructions:(NSArray<NSString*>*)instructions;

@end

#endif  // IOS_CHROME_BROWSER_INSTRUCTIONS_BOTTOM_SHEET_UI_INSTRUCTIONS_BOTTOM_SHEET_VIEW_CONTROLLER_H_
