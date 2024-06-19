// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_labeled_chip.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
static const CGFloat kLabelButtonSpacing = 2;
}  // namespace

@implementation ManualFillLabeledChip {
  UILabel* _label;
  NSArray<UIButton*>* _buttons;
}

#pragma mark - Public

- (id)initSingleChipWithTarget:(id)target selector:(SEL)action {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.spacing = kLabelButtonSpacing;
    self.axis = UILayoutConstraintAxisVertical;

    _label = CreateLabel();
    [self addArrangedSubview:_label];
    _buttons = @[ CreateChipWithSelectorAndTarget(action, target) ];
    [self addArrangedSubview:_buttons[0]];
  }
  return self;
}

- (id)initExpirationDateChipWithTarget:(id)target
                         monthSelector:(SEL)monthAction
                          yearSelector:(SEL)yearAction {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.spacing = kLabelButtonSpacing;
    self.axis = UILayoutConstraintAxisVertical;

    _label = CreateLabel();
    [self addArrangedSubview:_label];
    UIButton* monthButton =
        CreateChipWithSelectorAndTarget(monthAction, target);
    UIButton* yearButton = CreateChipWithSelectorAndTarget(yearAction, target);
    _buttons = @[ monthButton, yearButton ];

    UIStackView* dateStackView = [[UIStackView alloc] initWithFrame:CGRectZero];
    dateStackView.translatesAutoresizingMaskIntoConstraints = NO;
    dateStackView.spacing = GetHorizontalSpacingBetweenChips();
    dateStackView.axis = UILayoutConstraintAxisHorizontal;
    [dateStackView addArrangedSubview:monthButton];
    [dateStackView addArrangedSubview:[self createExpirationSeparatorLabel]];
    [dateStackView addArrangedSubview:yearButton];
    [self addArrangedSubview:dateStackView];
  }
  return self;
}

- (void)setLabelText:(NSString*)text
        buttonTitles:(NSArray<NSString*>*)buttonTitles {
  UIFont* font =
      [UIFont preferredFontForTextStyle:IsKeyboardAccessoryUpgradeEnabled()
                                            ? UIFontTextStyleCaption2
                                            : UIFontTextStyleFootnote];
  _label.attributedText = [[NSMutableAttributedString alloc]
      initWithString:[NSString stringWithFormat:@"%@", text]
          attributes:@{
            NSForegroundColorAttributeName :
                [UIColor colorNamed:kTextSecondaryColor],
            NSFontAttributeName : font
          }];
  _label.accessibilityIdentifier = text;

  CHECK_EQ(_buttons.count, buttonTitles.count);
  for (uint i = 0; i < _buttons.count; i++) {
    [_buttons[i] setTitle:buttonTitles[i] forState:UIControlStateNormal];
    _buttons[i].accessibilityIdentifier = buttonTitles[i];
  }
}

- (void)prepareForReuse {
  _label.text = @"";
  for (UIButton* button in _buttons) {
    [button setTitle:@"" forState:UIControlStateNormal];
  }
  self.hidden = NO;
}

- (UIButton*)singleButton {
  CHECK_EQ(_buttons.count, 1u);
  return _buttons[0];
}

- (UIButton*)expirationMonthButton {
  CHECK_EQ(_buttons.count, 2u);
  return _buttons[0];
}

- (UIButton*)expirationYearButton {
  CHECK_EQ(_buttons.count, 2u);
  return _buttons[1];
}

#pragma mark - Private

- (UILabel*)createExpirationSeparatorLabel {
  UILabel* expirationSeparatorLabel = CreateLabel();
  expirationSeparatorLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  [expirationSeparatorLabel setTextColor:[UIColor colorNamed:kSeparatorColor]];
  expirationSeparatorLabel.text = @"/";
  return expirationSeparatorLabel;
}

@end
