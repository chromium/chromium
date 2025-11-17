// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/instructions_bottom_sheet/instructions_bottom_sheet_view_controller.h"

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
  self.scrollEnabled = NO;
  self.dismissBarButtonSystemItem = UIBarButtonSystemItemClose;
  self.titleView = [self createTitleLabel];
  self.underTitleView = [self createInstructionView];
  [super viewDidLoad];

  [self setUpBottomSheetDetents];
  [self expandBottomSheet];
}

#pragma mark - Private

// Returns a label to use as the title of the instructions view.
- (UILabel*)createTitleLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 0;
  label.text = _titleString;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  label.adjustsFontForContentSizeCategory = YES;
  label.textAlignment = NSTextAlignmentCenter;
  label.accessibilityTraits |= UIAccessibilityTraitHeader;

  return label;
}

// Returns a view containing instructions to enable Enhanced Safe Browsing.
- (InstructionView*)createInstructionView {
  InstructionView* view = [[InstructionView alloc] initWithList:_steps];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  return view;
}

@end
