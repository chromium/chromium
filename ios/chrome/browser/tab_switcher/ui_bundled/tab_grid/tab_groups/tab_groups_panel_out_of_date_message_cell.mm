// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_out_of_date_message_cell.h"

#import "base/check_op.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const CGFloat kCornerRadius = 16;
const CGFloat kSymbolSize = 20;
const CGFloat kHorizontalPadding = 16;
const CGFloat kVerticalPadding = 20;
const CGFloat kTextLabelWidthMultiplier = 0.7;
const CGFloat kUpdateButtonWidthMultiplier = 0.4;

}  // namespace

@implementation TabGroupsPanelOutOfDateMessageCell {
  UILabel* _textLabel;
  UIButton* _updateButton;
  UIButton* _closeButton;

  NSArray<NSLayoutConstraint*>* _normalConstraints;
  NSArray<NSLayoutConstraint*>* _accessibilityConstraints;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
    self.layer.cornerRadius = kCornerRadius;

    UIView* contentView = self.contentView;

    _textLabel = [self createTextLabel];
    _updateButton = [self createUpdateButton];
    _closeButton = [self createCloseButton];

    [contentView addSubview:_textLabel];
    [contentView addSubview:_updateButton];
    [contentView addSubview:_closeButton];

    self.isAccessibilityElement = YES;
    self.accessibilityLabel = l10n_util::GetNSString(
        IDS_COLLABORATION_SHARED_TAB_GROUPS_PANEL_OUT_OF_DATE_MESSAGE_CELL_TEXT);
    self.accessibilityCustomActions = @[
      [[UIAccessibilityCustomAction alloc]
          initWithName:
              l10n_util::GetNSString(
                  IDS_IOS_TAB_GROUPS_PANEL_OUT_OF_DATE_MESSAGE_UPDATE_BUTTON)
                target:self
              selector:@selector(updateButtonTapped)],
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(IDS_IOS_ICON_CLOSE)
                target:self
              selector:@selector(closeButtonTapped)],

    ];

    _accessibilityConstraints = @[
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_closeButton.leadingAnchor
                                   constant:-kHorizontalPadding],
      [_textLabel.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                           constant:kVerticalPadding],
      [_updateButton.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_updateButton.topAnchor constraintEqualToAnchor:_textLabel.bottomAnchor
                                              constant:kVerticalPadding],
      [_updateButton.bottomAnchor
          constraintEqualToAnchor:contentView.bottomAnchor
                         constant:-kVerticalPadding],
      [_closeButton.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                             constant:kVerticalPadding],
    ];

    _normalConstraints = @[
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_updateButton.leadingAnchor],
      [_updateButton.trailingAnchor
          constraintEqualToAnchor:_closeButton.leadingAnchor
                         constant:-kHorizontalPadding],
      [_updateButton.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_textLabel.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_closeButton.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_textLabel.widthAnchor
          constraintLessThanOrEqualToAnchor:contentView.widthAnchor
                                 multiplier:kTextLabelWidthMultiplier],
      [_updateButton.widthAnchor
          constraintLessThanOrEqualToAnchor:contentView.widthAnchor
                                 multiplier:kUpdateButtonWidthMultiplier],
    ];

    [NSLayoutConstraint activateConstraints:@[
      [_closeButton.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [_closeButton.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:_updateButton.trailingAnchor
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
      [_updateButton.topAnchor
          constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                      constant:kVerticalPadding],
      [_updateButton.bottomAnchor
          constraintLessThanOrEqualToAnchor:contentView.bottomAnchor
                                   constant:-kVerticalPadding],
      [_closeButton.widthAnchor constraintEqualToConstant:kSymbolSize],
      [_closeButton.heightAnchor constraintEqualToConstant:kSymbolSize],
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
  _delegate = nil;
  _outOfDateMessageItem = nil;
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
  [label setContentHuggingPriority:UILayoutPriorityDefaultLow
                           forAxis:UILayoutConstraintAxisHorizontal];
  label.text = l10n_util::GetNSString(
      IDS_COLLABORATION_SHARED_TAB_GROUPS_PANEL_OUT_OF_DATE_MESSAGE_CELL_TEXT);
  return label;
}

// Returns a configured update button.
- (UIButton*)createUpdateButton {
  __weak __typeof(self) weakSelf = self;
  UIAction* updateAction = [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf updateButtonTapped];
  }];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  NSDirectionalEdgeInsets contentInsets = buttonConfiguration.contentInsets;
  contentInsets.leading = 0;
  contentInsets.trailing = 0;
  buttonConfiguration.contentInsets = contentInsets;
  UIButton* button = [UIButton buttonWithConfiguration:buttonConfiguration
                                         primaryAction:updateAction];
  [button
      setAttributedTitle:
          [[NSAttributedString alloc]
              initWithString:
                  l10n_util::GetNSString(
                      IDS_IOS_TAB_GROUPS_PANEL_OUT_OF_DATE_MESSAGE_UPDATE_BUTTON)
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

// Helper method to respond to update action.
- (void)updateButtonTapped {
  [self.delegate updateButtonTappedForOutOfDateMessageCell:self];
}

// Returns a configured close button.
- (UIButton*)createCloseButton {
  UIImage* buttonImage = DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolSize);
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  [buttonConfiguration setImage:buttonImage];
  __weak __typeof(self) weakSelf = self;
  UIAction* closeAction = [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf closeButtonTapped];
  }];
  UIButton* button = [UIButton buttonWithConfiguration:buttonConfiguration
                                         primaryAction:closeAction];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  button.accessibilityIdentifier =
      kTabGroupsPanelCloseOutOfDateMessageIdentifier;
  return button;
}

// Helper method to respond to close action.
- (void)closeButtonTapped {
  [self.delegate closeButtonTappedForOutOfDateMessageCell:self];
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
