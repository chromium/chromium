// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/instructions_bottom_sheet_view_controller.h"

#import "ios/chrome/browser/shared/ui/elements/instruction_view.h"

@implementation InstructionsBottomSheetViewController

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
  label.text = self.titleString;
  // Clear `titleString` so that ConfirmationAlertViewController doesn't use
  // it to create a label.
  self.titleString = nil;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  label.adjustsFontForContentSizeCategory = YES;
  label.textAlignment = NSTextAlignmentCenter;
  label.accessibilityTraits |= UIAccessibilityTraitHeader;

  return label;
}

// Returns a view containing instructions to enable Enhanced Safe Browsing.
- (InstructionView*)createInstructionView {
  InstructionView* view = [[InstructionView alloc] initWithList:self.steps];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  return view;
}

@end
