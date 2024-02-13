// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_INSTRUCTION_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_INSTRUCTION_VIEW_H_

#import <UIKit/UIKit.h>

// Enum defining different styles for the instruction view.
typedef NS_ENUM(NSInteger, InstructionViewStyle) {
  InstructionViewStyleDefault = 0,
  InstructionViewStyleGrayscale
};

// View containing an instruction list with their step number.
@interface InstructionView : UIView

// Creates the numbered instructions view list with `instructionList` which
// contains instructions strings. Strings can have bold part in it.
// `instructionList` must have at least one step.
// `style` should be one of InstructionViewStyle.
// `iconViews` optional, should be supplied if a view should be used instead of
// step number. If provided, the number of icons should be the same as the
// number of steps in instructionList.
- (instancetype)initWithList:(NSArray<NSString*>*)instructionList
                       style:(InstructionViewStyle)style
                   iconViews:(NSArray<UIView*>*)iconViews
    NS_DESIGNATED_INITIALIZER;

// Creates icon views and calls -[InstructionView
// initWithList:style:iconViews:].
- (instancetype)initWithList:(NSArray<NSString*>*)instructionList
                       style:(InstructionViewStyle)style
                       icons:(NSArray<UIImage*>*)icons;

// Creates the numbered instructions without icons.
- (instancetype)initWithList:(NSArray<NSString*>*)instructionList
                       style:(InstructionViewStyle)style;

// Creates the numbered instructions view with InstructionViewStyleDefault.
- (instancetype)initWithList:(NSArray<NSString*>*)instructionList;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_INSTRUCTION_VIEW_H_
