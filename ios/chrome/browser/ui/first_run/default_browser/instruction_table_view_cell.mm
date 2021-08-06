// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/default_browser/instruction_table_view_cell.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/common/ui/util/dynamic_type_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr CGFloat kStepNumberLabelSize = 20;

}  // namespace

@interface InstructionTableViewCell ()

// Step number of the instruction.
@property(strong, nonatomic) UILabel* stepNumberLabel;

@end

@implementation InstructionTableViewCell

#pragma mark - UITableViewCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.textLabel.textColor = [UIColor colorNamed:kGrey800Color];
    self.textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    self.selectionStyle = UITableViewCellSelectionStyleNone;

    UIView* stepNumberView = [self createStepNumberView];
    [self.contentView addSubview:stepNumberView];

    [NSLayoutConstraint activateConstraints:@[
      [stepNumberView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [stepNumberView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];
  }
  return self;
}

#pragma mark - InstructionTableViewCell

- (void)configureCellText:(NSAttributedString*)instructionText
           withStepNumber:(NSInteger)instructionStepNumber {
  self.textLabel.attributedText = instructionText;
  self.stepNumberLabel.text =
      [NSString stringWithFormat:@"%ld", instructionStepNumber];
}

#pragma mark - Private

// Creates a view with a round numbered label in it.
- (UIView*)createStepNumberView {
  self.stepNumberLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  self.stepNumberLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.stepNumberLabel.textColor = [UIColor colorNamed:kBlueColor];
  self.stepNumberLabel.textAlignment = NSTextAlignmentCenter;
  self.stepNumberLabel.font = PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleFootnote,
      self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryExtraExtraExtraLarge);

  UIFontDescriptor* boldFontDescriptor =
      [self.stepNumberLabel.font.fontDescriptor
          fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  self.stepNumberLabel.font = [UIFont fontWithDescriptor:boldFontDescriptor
                                                    size:0];

  self.stepNumberLabel.layer.cornerRadius = kStepNumberLabelSize / 2;
  self.stepNumberLabel.layer.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor].CGColor;

  UIView* labelContainer = [[UIView alloc] initWithFrame:CGRectZero];
  labelContainer.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  labelContainer.translatesAutoresizingMaskIntoConstraints = NO;

  [labelContainer addSubview:self.stepNumberLabel];

  [NSLayoutConstraint activateConstraints:@[
    [self.stepNumberLabel.centerYAnchor
        constraintEqualToAnchor:labelContainer.centerYAnchor],
    [self.stepNumberLabel.centerXAnchor
        constraintEqualToAnchor:labelContainer.centerXAnchor],
    [self.stepNumberLabel.widthAnchor
        constraintEqualToConstant:kStepNumberLabelSize],
    [self.stepNumberLabel.heightAnchor
        constraintEqualToConstant:kStepNumberLabelSize],

    [labelContainer.widthAnchor
        constraintEqualToConstant:kTableViewIconImageSize],
    [labelContainer.heightAnchor
        constraintEqualToAnchor:labelContainer.widthAnchor],
  ]];

  return labelContainer;
}

@end
