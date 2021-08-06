// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_DEFAULT_BROWSER_INSTRUCTION_TABLE_VIEW_CELL_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_DEFAULT_BROWSER_INSTRUCTION_TABLE_VIEW_CELL_H_

#import <UIKit/UIKit.h>

// Base class for the TableViewCell used by the TableViewItems.
@interface InstructionTableViewCell : UITableViewCell

// Configures instruction text and step number.
- (void)configureCellText:(NSAttributedString*)instructionText
           withStepNumber:(NSInteger)instructionStepNumber;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_DEFAULT_BROWSER_INSTRUCTION_TABLE_VIEW_CELL_H_
