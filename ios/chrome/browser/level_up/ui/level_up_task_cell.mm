// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_task_cell.h"

#import "ios/chrome/browser/level_up/ui/level_up_task.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Padding spacing constant.
const CGFloat kLayoutSpacing = 16.0;
// The point size of a task icon.
const CGFloat kIconSize = 24.0;
// The point size of status chevron indicators.
const CGFloat kChevronSize = 14.0;
// The spacing within the vertical text container stack.
const CGFloat kTextContainerSpacing = 2.0;

}  // namespace

@implementation LevelUpTaskCell {
  // Status icon displaying the completion checkmark or task icon template.
  UIImageView* _iconView;
  // Label displaying the title of the task.
  UILabel* _titleLabel;
  // Label displaying the description of the task.
  UILabel* _descriptionLabel;
  // Right chevron indicator view.
  UIImageView* _chevronView;
  // Vertical stack view wrapping the title and description labels.
  UIStackView* _textContainer;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
    self.selectionStyle = UITableViewCellSelectionStyleNone;

    _iconView = [self createIconView];
    _titleLabel = [self createTitleLabel];
    _descriptionLabel = [self createDescriptionLabel];
    _chevronView = [self createChevronView];

    _textContainer = [self createTextContainerWithTitle:_titleLabel
                                            description:_descriptionLabel];

    UIStackView* rowStackView =
        [self createRowStackViewWithTextContainer:_textContainer];

    [self setupLayoutConstraintsWithRowStackView:rowStackView];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGRect textContainerFrame =
      [self.contentView convertRect:_textContainer.frame
                           fromView:_textContainer.superview];
  self.separatorInset = UIEdgeInsetsMake(0, textContainerFrame.origin.x, 0, 0);
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _iconView.image = nil;
  _titleLabel.text = nil;
  _descriptionLabel.text = nil;
}

- (void)configureWithTask:(LevelUpTask*)task {
  if (task.completed) {
    _iconView.tintColor = [UIColor colorNamed:kGreen600Color];
    _iconView.image = DefaultSymbolWithPointSize(kCheckmarkSymbol, kIconSize);
  } else {
    _iconView.tintColor = [UIColor colorNamed:kBlueColor];
    _iconView.image =
        DefaultSymbolWithPointSize(task.iconSymbolName, kIconSize);
  }

  _titleLabel.text = task.title;
  _descriptionLabel.text = task.taskDescription;
}

#pragma mark - Private

// Creates the status icon view.
- (UIImageView*)createIconView {
  UIImageView* iconView = [[UIImageView alloc] init];
  iconView.translatesAutoresizingMaskIntoConstraints = NO;
  iconView.contentMode = UIViewContentModeScaleAspectFit;
  AddSquareConstraints(iconView, kIconSize);
  return iconView;
}

// Creates the title label.
- (UILabel*)createTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  titleLabel.numberOfLines = 0;
  return titleLabel;
}

// Creates the description label.
- (UILabel*)createDescriptionLabel {
  UILabel* descriptionLabel = [[UILabel alloc] init];
  descriptionLabel.translatesAutoresizingMaskIntoConstraints = NO;
  descriptionLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  descriptionLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  descriptionLabel.numberOfLines = 0;
  descriptionLabel.textAlignment = NSTextAlignmentNatural;
  return descriptionLabel;
}

// Creates the chevron accessory view.
- (UIImageView*)createChevronView {
  UIImageView* chevronView = [[UIImageView alloc] init];
  chevronView.translatesAutoresizingMaskIntoConstraints = NO;
  chevronView.contentMode = UIViewContentModeScaleAspectFit;
  chevronView.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
  chevronView.image =
      DefaultSymbolWithPointSize(kChevronForwardSymbol, kChevronSize);
  AddSquareConstraints(chevronView, kChevronSize);

  [chevronView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [chevronView setContentHuggingPriority:UILayoutPriorityRequired
                                 forAxis:UILayoutConstraintAxisHorizontal];
  return chevronView;
}

// Creates the vertical stack view wrapping the title and description labels.
- (UIStackView*)createTextContainerWithTitle:(UIView*)titleLabel
                                 description:(UIView*)descriptionLabel {
  UIStackView* textContainer = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, descriptionLabel ]];
  textContainer.axis = UILayoutConstraintAxisVertical;
  textContainer.spacing = kTextContainerSpacing;
  textContainer.translatesAutoresizingMaskIntoConstraints = NO;
  return textContainer;
}

// Creates the horizontal row stack view wrapping the elements.
- (UIStackView*)createRowStackViewWithTextContainer:(UIView*)textContainer {
  UIStackView* rowStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _iconView, textContainer, _chevronView ]];
  rowStackView.axis = UILayoutConstraintAxisHorizontal;
  rowStackView.spacing = kLayoutSpacing;
  rowStackView.alignment = UIStackViewAlignmentCenter;
  rowStackView.translatesAutoresizingMaskIntoConstraints = NO;
  return rowStackView;
}

// Sets up constraints to pin the row stack view inside the cell's content view.
- (void)setupLayoutConstraintsWithRowStackView:(UIView*)rowStackView {
  [self.contentView addSubview:rowStackView];
  AddSameConstraintsWithInsets(
      rowStackView, self.contentView,
      NSDirectionalEdgeInsetsMake(kLayoutSpacing, kLayoutSpacing,
                                  kLayoutSpacing, kLayoutSpacing));
}

@end
