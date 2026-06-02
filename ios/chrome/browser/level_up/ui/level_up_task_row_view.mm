// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_task_row_view.h"

#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"
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
// The fixed height of each task checklist row view.
const CGFloat kRowHeight = 75.0;

}  // namespace

@implementation LevelUpTaskRowView {
  // Icon showing task completion state.
  UIImageView* _iconView;
  // Task title label.
  UILabel* _titleLabel;
  // Task description label.
  UILabel* _descriptionLabel;
  // Chevron indicating row tap action.
  UIImageView* _chevronView;
  // Container for title and description.
  UIStackView* _textContainer;
  // Line separating rows.
  UIView* _separatorView;
  // Navigation action on tap.
  void (^_navigationAction)(void);
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _iconView = [[UIImageView alloc] init];
    _iconView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconView.contentMode = UIViewContentModeScaleAspectFit;
    AddSquareConstraints(_iconView, kIconSize);

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.numberOfLines = 0;

    _descriptionLabel = [[UILabel alloc] init];
    _descriptionLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _descriptionLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _descriptionLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _descriptionLabel.numberOfLines = 0;

    _chevronView = [[UIImageView alloc] init];
    _chevronView.translatesAutoresizingMaskIntoConstraints = NO;
    _chevronView.contentMode = UIViewContentModeScaleAspectFit;
    _chevronView.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
    _chevronView.image =
        DefaultSymbolWithPointSize(kChevronForwardSymbol, kChevronSize);
    AddSquareConstraints(_chevronView, kChevronSize);
    [_chevronView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_chevronView setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisHorizontal];

    _textContainer = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _descriptionLabel ]];
    _textContainer.axis = UILayoutConstraintAxisVertical;
    _textContainer.spacing = kTextContainerSpacing;
    _textContainer.translatesAutoresizingMaskIntoConstraints = NO;

    UIStackView* rowStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _iconView, _textContainer, _chevronView ]];
    rowStack.axis = UILayoutConstraintAxisHorizontal;
    rowStack.spacing = kLayoutSpacing;
    rowStack.alignment = UIStackViewAlignmentCenter;
    rowStack.translatesAutoresizingMaskIntoConstraints = NO;
    rowStack.userInteractionEnabled = NO;

    _separatorView = [[UIView alloc] init];
    _separatorView.translatesAutoresizingMaskIntoConstraints = NO;
    _separatorView.backgroundColor =
        [[UIColor colorNamed:kSeparatorColor] colorWithAlphaComponent:0.4];

    [self addSubview:rowStack];
    [self addSubview:_separatorView];

    AddSameConstraintsWithInsets(
        rowStack, self,
        NSDirectionalEdgeInsetsMake(kLayoutSpacing, kLayoutSpacing,
                                    kLayoutSpacing, kLayoutSpacing));

    [NSLayoutConstraint activateConstraints:@[
      [self.heightAnchor constraintGreaterThanOrEqualToConstant:kRowHeight],
      [_separatorView.heightAnchor constraintEqualToConstant:1.0],
      [_separatorView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [_separatorView.leadingAnchor
          constraintEqualToAnchor:_textContainer.leadingAnchor],
      [_separatorView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
    ]];

    [self addTarget:self
                  action:@selector(didTapRow)
        forControlEvents:UIControlEventTouchUpInside];
  }
  return self;
}

- (void)configureWithTask:(LevelUpTask*)task showSeparator:(BOOL)showSeparator {
  _navigationAction = task.navigationAction;

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
  _separatorView.hidden = !showSeparator;
}

- (void)didTapRow {
  if (_navigationAction) {
    _navigationAction();
  }
}

@end
