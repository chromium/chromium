// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/tab_group_activity_summary_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const CGFloat kCornerRadius = 16;
const CGFloat kSymbolSize = 14;
const CGFloat kCloseButtonSize = 20;
const CGFloat kHorizontalPadding = 16;
const CGFloat kVerticalPadding = 8;
const CGFloat kTextLabelWidthMultiplier = 0.7;
const CGFloat kActivityButtonWidthMultiplier = 0.4;

}  // namespace

@implementation TabGroupActivitySummaryCell {
  UILabel* _textLabel;
  UIButton* _activityButton;
  UIButton* _closeButton;

  NSArray<NSLayoutConstraint*>* _normalConstraints;
  NSArray<NSLayoutConstraint*>* _accessibilityConstraints;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIBlurEffect* blurEffect = [UIBlurEffect
        effectWithStyle:UIBlurEffectStyleSystemUltraThinMaterialDark];
    UIVisualEffectView* backgroundView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    backgroundView.backgroundColor = TabGroupViewButtonBackgroundColor();
    backgroundView.frame = self.bounds;
    backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:backgroundView];
    AddSameConstraints(self, backgroundView);

    self.layer.cornerRadius = kCornerRadius;
    self.layer.masksToBounds = YES;

    UIView* contentView = self.contentView;

    _textLabel = [self createTextLabel];
    _activityButton = [self createActivityButton];
    _closeButton = [self createCloseButton];

    [contentView addSubview:_textLabel];
    [contentView addSubview:_activityButton];
    [contentView addSubview:_closeButton];

    _accessibilityConstraints = @[
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_closeButton.leadingAnchor
                                   constant:-kHorizontalPadding],
      [_textLabel.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                           constant:kVerticalPadding],
      [_activityButton.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_activityButton.topAnchor constraintEqualToAnchor:_textLabel.bottomAnchor
                                                constant:kVerticalPadding],
      [_activityButton.bottomAnchor
          constraintEqualToAnchor:contentView.bottomAnchor
                         constant:-kVerticalPadding],
      [_closeButton.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                             constant:kVerticalPadding],
    ];

    _normalConstraints = @[
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_activityButton.leadingAnchor],
      [_activityButton.trailingAnchor
          constraintEqualToAnchor:_closeButton.leadingAnchor
                         constant:-kHorizontalPadding],
      [_activityButton.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_textLabel.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_closeButton.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_textLabel.widthAnchor
          constraintLessThanOrEqualToAnchor:contentView.widthAnchor
                                 multiplier:kTextLabelWidthMultiplier],
      [_activityButton.widthAnchor
          constraintLessThanOrEqualToAnchor:contentView.widthAnchor
                                 multiplier:kActivityButtonWidthMultiplier],
    ];

    [NSLayoutConstraint activateConstraints:@[
      [_closeButton.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [_closeButton.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:_activityButton.trailingAnchor
                                      constant:kHorizontalPadding],
      [_textLabel.topAnchor
          constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                      constant:kVerticalPadding],
      [_textLabel.bottomAnchor
          constraintLessThanOrEqualToAnchor:contentView.bottomAnchor
                                   constant:-kVerticalPadding],
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_activityButton.topAnchor
          constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                      constant:kVerticalPadding],
      [_activityButton.bottomAnchor
          constraintLessThanOrEqualToAnchor:contentView.bottomAnchor
                                   constant:-kVerticalPadding],
      [_closeButton.widthAnchor constraintEqualToConstant:kCloseButtonSize],
      [_closeButton.heightAnchor constraintEqualToConstant:kCloseButtonSize],
    ]];

    [self updateConstraintsForContentSizeCategory];

    [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.class ]
                       withAction:@selector
                       (updateConstraintsForContentSizeCategory)];
  }

  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _textLabel.text = nil;
  _delegate = nil;
  _text = nil;
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
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
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
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  NSDirectionalEdgeInsets contentInsets = buttonConfiguration.contentInsets;
  contentInsets.leading = 0;
  contentInsets.trailing = 0;
  buttonConfiguration.contentInsets = contentInsets;
  UIButton* button = [UIButton buttonWithConfiguration:buttonConfiguration
                                         primaryAction:openActivityAction];
  [button setAttributedTitle:
              [[NSAttributedString alloc]
                  initWithString:
                      l10n_util::GetNSString(
                          IDS_IOS_TAB_GROUP_ACTIVITY_SUMMARY_ACTIVITY_BUTTON)
                      attributes:@{
                        NSFontAttributeName : PreferredFontForTextStyle(
                            UIFontTextStyleSubheadline, UIFontWeightSemibold)
                      }]
                    forState:UIControlStateNormal];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  button.titleLabel.adjustsFontSizeToFitWidth = YES;
  button.titleLabel.maximumContentSizeCategory =
      UIContentSizeCategoryAccessibilityLarge;
  // Make sure that the button has priority over the text.
  [button
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisHorizontal];
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
  button.accessibilityIdentifier =
      kActivitySummaryGridCellCloseButtonIdentifier;
  button.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_TAB_GROUP_ACTIVITY_SUMMARY_ACTIVITY_CLOSE_BUTTON_ACCESSIBILITY_LABEL);
  return button;
}

// Updates the constraints to be appropriate for the current content size
// category.
- (void)updateConstraintsForContentSizeCategory {
  if (UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory)) {
    [NSLayoutConstraint deactivateConstraints:_normalConstraints];
    [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_accessibilityConstraints];
    [NSLayoutConstraint activateConstraints:_normalConstraints];
  }
}

@end
