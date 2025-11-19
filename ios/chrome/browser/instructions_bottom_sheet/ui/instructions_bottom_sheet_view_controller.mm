// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/instructions_bottom_sheet/ui/instructions_bottom_sheet_view_controller.h"

#import "ios/chrome/common/ui/instruction_view/instruction_view.h"

@implementation InstructionsBottomSheetViewController {
  // The title of the instructions view.
  NSString* _titleString;
  // An array containing the strings of each step to display in the
  // instructions.
  NSArray<NSString*>* _steps;
}

- (instancetype)initWithTitle:(NSString*)title
                 instructions:(NSArray<NSString*>*)instructions {
  self = [super init];
  if (self) {
    _titleString = title;
    _steps = instructions;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.topAlignedLayout = YES;
  self.title = _titleString;
  self.underTitleView = [self createInstructionView];
  [super viewDidLoad];

  [self setUpBottomSheetDetents];
}

#pragma mark - Private

// Returns a view containing instructions to enable Enhanced Safe Browsing.
- (InstructionView*)createInstructionView {
  InstructionView* view = [[InstructionView alloc] initWithList:_steps];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  return view;
}

@end
