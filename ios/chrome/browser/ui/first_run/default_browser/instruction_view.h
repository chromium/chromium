// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_DEFAULT_BROWSER_INSTRUCTION_VIEW_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_DEFAULT_BROWSER_INSTRUCTION_VIEW_H_

#import <UIKit/UIKit.h>

// View containing an instruction list with their step number.
@interface InstructionView : UIView

// Creates the numbered instructions view list with |instructionList| which
// contains instructions strings. Strings can have bold part in it.
// |instructionList| must have at least one step.
- (instancetype)initWithList:(NSArray<NSString*>*)instructionList;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_DEFAULT_BROWSER_INSTRUCTION_VIEW_H_
