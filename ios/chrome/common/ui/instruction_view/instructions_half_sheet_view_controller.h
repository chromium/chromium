// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_INSTRUCTION_VIEW_INSTRUCTIONS_HALF_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_COMMON_UI_INSTRUCTION_VIEW_INSTRUCTIONS_HALF_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ConfirmationAlertActionHandler;

// View controller for the half screen instructions.
@interface InstructionsHalfSheetViewController : UIViewController

// Initializes a Instructions Half Sheet View Controller with
// `instructionList` and `actionHandler`.
- (instancetype)initWithInstructionList:(NSArray<NSString*>*)instructionList
                          actionHandler:
                              (id<ConfirmationAlertActionHandler>)actionHandler;

// The title text of the half sheet, defaults to "Show Me How" if nil.
@property(nonatomic, weak) NSString* titleText;

@end

#endif  // IOS_CHROME_COMMON_UI_INSTRUCTION_VIEW_INSTRUCTIONS_HALF_SHEET_VIEW_CONTROLLER_H_
