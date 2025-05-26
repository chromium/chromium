// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_notification_cell.h"

#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
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

}  // namespace

@implementation TabGroupsPanelNotificationCell {
  UILabel* _textLabel;
  UIButton* _closeButton;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
    self.layer.cornerRadius = kCornerRadius;
    self.isAccessibilityElement = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;
    self.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_TAB_GROUPS_PANEL_NOTIFICATION_CELL_ACCESSIBILITY_HINT);

    UIView* contentView = self.contentView;

    _textLabel = [self createTextLabel];
    _closeButton = [self createCloseButton];

    UIStackView* stackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textLabel, _closeButton ]];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.axis = UILayoutConstraintAxisHorizontal;
    stackView.distribution = UIStackViewDistributionFill;
    stackView.alignment = UIStackViewAlignmentCenter;
    [contentView addSubview:stackView];

    [NSLayoutConstraint activateConstraints:@[
      [stackView.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                              constant:kHorizontalPadding],
      [stackView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [stackView.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                          constant:kVerticalPadding],
      [stackView.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor
                                             constant:-kVerticalPadding],
      [_closeButton.widthAnchor constraintEqualToConstant:kSymbolSize],
      [_closeButton.heightAnchor constraintEqualToConstant:kSymbolSize],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _textLabel.text = nil;
  _delegate = nil;
  _notificationItem = nil;
}

- (NSString*)accessibilityLabel {
  return _textLabel.text;
}

- (void)setNotificationItem:(TabGroupsPanelItem*)notificationItem {
  CHECK_EQ(notificationItem.type, TabGroupsPanelItemType::kNotification);
  if ([notificationItem isEqual:_notificationItem]) {
    return;
  }
  _notificationItem = notificationItem;
  _textLabel.text = notificationItem.notificationText;
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
  return label;
}

// Returns a configured close button.
- (UIButton*)createCloseButton {
  UIImage* buttonImage = DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolSize);
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  [buttonConfiguration setImage:buttonImage];
  __weak __typeof(self) weakSelf = self;
  UIAction* closeAction = [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.delegate
        closeButtonTappedForNotificationItem:weakSelf.notificationItem];
  }];
  UIButton* button =
      [ExtendedTouchTargetButton buttonWithConfiguration:buttonConfiguration
                                           primaryAction:closeAction];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  button.accessibilityIdentifier = kTabGroupsPanelCloseNotificationIdentifier;
  return button;
}

@end
