// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_task_row_view.h"

#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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
// The duration for chevron rotation animations.
const NSTimeInterval kChevronAnimationDuration = 0.25;

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
  // Stack view holding the row components.
  UIStackView* _rowStack;
  // Line separating rows.
  UIView* _separatorView;
  // Backing task model.
  __weak LevelUpTask* _task;
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
        SymbolWithPointSize(SymbolChevronForward, kChevronSize);
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

    _rowStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _iconView, _textContainer, _chevronView ]];
    _rowStack.axis = UILayoutConstraintAxisHorizontal;
    _rowStack.spacing = kLayoutSpacing;
    _rowStack.alignment = UIStackViewAlignmentCenter;
    _rowStack.translatesAutoresizingMaskIntoConstraints = NO;
    _rowStack.userInteractionEnabled = NO;

    _separatorView = [[UIView alloc] init];
    _separatorView.translatesAutoresizingMaskIntoConstraints = NO;
    _separatorView.backgroundColor =
        [[UIColor colorNamed:kSeparatorColor] colorWithAlphaComponent:0.4];

    [self addSubview:_rowStack];
    [self addSubview:_separatorView];

    AddSameConstraintsWithInsets(
        _rowStack, self,
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
  self.backgroundColor = nil;
  _chevronView.transform = CGAffineTransformIdentity;
  _task = task;

  _iconView.hidden = NO;
  [_rowStack setCustomSpacing:UIStackViewSpacingUseDefault
                    afterView:_textContainer];

  if (task.completed) {
    _iconView.tintColor = [UIColor colorNamed:kGreen600Color];
    _iconView.image = SymbolWithPointSize(SymbolCheckmark, kIconSize);
  } else {
    _iconView.tintColor = [UIColor colorNamed:kBlueColor];
    if (task.isCustomSymbol) {
      _iconView.image =
          CustomSymbolWithPointSize(task.iconSymbolName, kIconSize);
    } else {
      _iconView.image =
          DefaultSymbolWithPointSize(task.iconSymbolName, kIconSize);
    }
  }

  _titleLabel.text = task.title;
  _descriptionLabel.text = task.taskDescription;
  _separatorView.hidden = !showSeparator;
}

- (void)configureWithTitle:(NSString*)title
               description:(NSString*)description
                      icon:(UIImage*)icon
           backgroundColor:(UIColor*)backgroundColor
           chevronExpanded:(BOOL)chevronExpanded
           separatorHidden:(BOOL)separatorHidden {
  self.backgroundColor = backgroundColor;
  _task = nil;

  if (icon) {
    _iconView.image = icon;
    _iconView.hidden = NO;
    [_rowStack setCustomSpacing:UIStackViewSpacingUseDefault
                      afterView:_textContainer];
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  } else {
    _iconView.image = nil;
    _iconView.hidden = YES;
    [_rowStack setCustomSpacing:UIStackViewSpacingUseSystem
                      afterView:_textContainer];
    _titleLabel.textColor = [UIColor colorNamed:kGrey700Color];
    _titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  }

  _titleLabel.text = title;
  _descriptionLabel.text = description;
  _separatorView.hidden = separatorHidden;

  CGFloat angle = chevronExpanded ? M_PI_2 : 0.0;
  _chevronView.transform = CGAffineTransformMakeRotation(angle);
}

- (void)setChevronExpanded:(BOOL)expanded animated:(BOOL)animated {
  UIImageView* chevronView = _chevronView;
  void (^animations)(void) = ^{
    CGFloat angle = expanded ? M_PI_2 : 0.0;
    chevronView.transform = CGAffineTransformMakeRotation(angle);
  };

  if (animated) {
    [UIView animateWithDuration:kChevronAnimationDuration
                     animations:animations];
  } else {
    animations();
  }
}

- (void)setSeparatorHidden:(BOOL)hidden {
  _separatorView.hidden = hidden;
}

- (void)didTapRow {
  [self.delegate taskRowView:self didTapTask:_task];
}

@end
