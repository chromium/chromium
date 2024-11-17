// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_group_activity_summary_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const CGFloat kCornerRadius = 16;
const CGFloat kSymbolSize = 20;
const CGFloat kHorizontalPadding = 16;
const CGFloat kVerticalPadding = 16;
const CGFloat kTextLabelWidthMultiplier = 0.7;
const CGFloat kActivityButtonWidthMultiplier = 0.4;

}  // namespace

@implementation TabGroupActivitySummaryCell {
  UILabel* _textLabel;
  UIButton* _activityButton;
  UIButton* _closeButton;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kTertiaryBackgroundColor];
    self.layer.cornerRadius = kCornerRadius;

    UIView* contentView = self.contentView;

    _textLabel = [self createTextLabel];
    _activityButton = [self createActivityButton];
    _closeButton = [self createCloseButton];

    UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _textLabel, _activityButton, _closeButton
    ]];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.axis = UILayoutConstraintAxisHorizontal;
    stackView.distribution = UIStackViewDistributionFill;
    stackView.alignment = UIStackViewAlignmentCenter;
    [stackView setCustomSpacing:kHorizontalPadding afterView:_activityButton];
    [contentView addSubview:stackView];

    [NSLayoutConstraint activateConstraints:@[
      [stackView.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                              constant:kHorizontalPadding],
      [stackView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [stackView.topAnchor
          constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                      constant:kVerticalPadding],
      [stackView.bottomAnchor
          constraintLessThanOrEqualToAnchor:contentView.bottomAnchor
                                   constant:-kVerticalPadding],

      [_textLabel.widthAnchor
          constraintLessThanOrEqualToAnchor:stackView.widthAnchor
                                 multiplier:kTextLabelWidthMultiplier],
      [_activityButton.widthAnchor
          constraintLessThanOrEqualToAnchor:stackView.widthAnchor
                                 multiplier:kActivityButtonWidthMultiplier],
      [_closeButton.widthAnchor constraintEqualToConstant:kSymbolSize],
      [_closeButton.heightAnchor constraintEqualToConstant:kSymbolSize],
    ]];
  }

  return self;
}

- (void)setText:(NSString*)text {
  if ([_text isEqualToString:text]) {
    return;
  }
  _text = [text copy];
  _textLabel.text = _text;
}

#pragma mark - Private

// Returns a configured text label.
- (UILabel*)createTextLabel {
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.adjustsFontForContentSizeCategory = YES;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.textColor = UIColor.whiteColor;
  label.numberOfLines = 0;
  return label;
}

// Returns a configured activity button.
- (UIButton*)createActivityButton {
  __weak __typeof(self) weakSelf = self;
  UIAction* openActivityAction =
      [UIAction actionWithHandler:^(UIAction* action) {
        [weakSelf.delegate activityButtonForActivitySummaryTapped];
      }];
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem
                                primaryAction:openActivityAction];
  [button setAttributedTitle:
              [[NSAttributedString alloc]
                  initWithString:
                      l10n_util::GetNSString(
                          IDS_IOS_TAB_GROUP_ACTIVITY_SUMMARY_ACTIVITY_BUTTON)
                      attributes:@{
                        NSFontAttributeName : CreateDynamicFont(
                            UIFontTextStyleSubheadline, UIFontWeightSemibold)
                      }]
                    forState:UIControlStateNormal];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.adjustsFontSizeToFitWidth = YES;
  return button;
}

// Returns a configured close button.
- (UIButton*)createCloseButton {
  UIImage* buttonImage = DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolSize);
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  [buttonConfiguration setImage:buttonImage];
  __weak __typeof(self) weakSelf = self;
  UIAction* closeAction = [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.delegate closeButtonForActivitySummaryTapped];
  }];
  UIButton* button = [UIButton buttonWithConfiguration:buttonConfiguration
                                         primaryAction:closeAction];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  return button;
}

@end
