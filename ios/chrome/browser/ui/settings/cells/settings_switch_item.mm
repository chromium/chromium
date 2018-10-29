// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"

#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used between the icon and the text labels.
const CGFloat kIconTrailingPadding = 12;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Size of the icon image.
const CGFloat kIconImageSize = 28;
}  // namespace

@implementation SettingsSwitchItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsSwitchCell class];
    self.enabled = YES;
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(SettingsSwitchCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  cell.switchView.enabled = self.enabled;
  cell.switchView.on = self.on;
  cell.textLabel.textColor =
      [SettingsSwitchCell defaultTextColorForState:cell.switchView.state];

  // Update the icon image, if one is present.
  UIImage* iconImage = nil;
  if ([self.iconImageName length]) {
    iconImage = [UIImage imageNamed:self.iconImageName];
  }
  [cell setIconImage:iconImage];
}

@end

#pragma mark - SettingsSwitchCell

@interface SettingsSwitchCell ()

// The image view for the leading icon.
@property(nonatomic, readonly, strong) UIImageView* iconImageView;

// Constraints that are used when the iconImageView is visible and hidden.
@property(nonatomic, strong) NSLayoutConstraint* iconVisibleConstraint;
@property(nonatomic, strong) NSLayoutConstraint* iconHiddenConstraint;

// Constraints that are used when the preferred content size is an
// "accessibility" category.
@property(nonatomic, strong) NSArray* accessibilityConstraints;
// Constraints that are used when the preferred content size is *not* an
// "accessibility" category.
@property(nonatomic, strong) NSArray* standardConstraints;

@end

@implementation SettingsSwitchCell

@synthesize accessibilityConstraints = _accessibilityConstraints;
@synthesize standardConstraints = _standardConstraints;
@synthesize iconHiddenConstraint = _iconHiddenConstraint;
@synthesize iconVisibleConstraint = _iconVisibleConstraint;
@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;

    _iconImageView = [[UIImageView alloc] init];
    _iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconImageView.hidden = YES;
    [self.contentView addSubview:_iconImageView];

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor blackColor];
    _textLabel.numberOfLines = 0;
    [self.contentView addSubview:_textLabel];

    _switchView = [[UISwitch alloc] initWithFrame:CGRectZero];
    _switchView.translatesAutoresizingMaskIntoConstraints = NO;
    _switchView.onTintColor = UIColorFromRGB(kTableViewSwitchTintColor);
    _switchView.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_TOGGLE_SETTING_SWITCH_ACCESSIBILITY_HINT);
    [self.contentView addSubview:_switchView];

    // Set up the constraints assuming that the icon image is hidden.
    _iconVisibleConstraint = [_textLabel.leadingAnchor
        constraintEqualToAnchor:_iconImageView.trailingAnchor
                       constant:kIconTrailingPadding];
    _iconHiddenConstraint = [_textLabel.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing];

    _standardConstraints = @[
      [_switchView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_switchView.leadingAnchor
                                   constant:-kTableViewHorizontalSpacing],
    ];
    _accessibilityConstraints = @[
      [_switchView.topAnchor constraintEqualToAnchor:_textLabel.bottomAnchor
                                            constant:kVerticalPadding],
      [_switchView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_switchView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kVerticalPadding],
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.trailingAnchor
                                   constant:-kTableViewHorizontalSpacing],
    ];

    [NSLayoutConstraint activateConstraints:@[
      [_iconImageView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_iconImageView.widthAnchor constraintEqualToConstant:kIconImageSize],
      [_iconImageView.heightAnchor constraintEqualToConstant:kIconImageSize],

      [_switchView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],

      [_iconImageView.centerYAnchor
          constraintEqualToAnchor:_textLabel.centerYAnchor],

      _iconHiddenConstraint,
    ]];

    if (ContentSizeCategoryIsAccessibilityCategory(
            self.traitCollection.preferredContentSizeCategory)) {
      [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
    } else {
      [NSLayoutConstraint activateConstraints:_standardConstraints];
    }

    AddOptionalVerticalPadding(self.contentView, _textLabel, kVerticalPadding);
  }
  return self;
}

+ (UIColor*)defaultTextColorForState:(UIControlState)state {
  return (state & UIControlStateDisabled)
             ? UIColorFromRGB(kSettingsCellsDetailTextColor)
             : [UIColor blackColor];
}

- (void)setIconImage:(UIImage*)image {
  BOOL hidden = (image == nil);
  if (hidden == self.iconImageView.hidden) {
    return;
  }

  self.iconImageView.image = image;
  self.iconImageView.hidden = hidden;
  if (hidden) {
    self.iconVisibleConstraint.active = NO;
    self.iconHiddenConstraint.active = YES;
  } else {
    self.iconHiddenConstraint.active = NO;
    self.iconVisibleConstraint.active = YES;
  }
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  BOOL isCurrentContentSizeAccessibility =
      ContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (ContentSizeCategoryIsAccessibilityCategory(
          previousTraitCollection.preferredContentSizeCategory) !=
      isCurrentContentSizeAccessibility) {
    if (isCurrentContentSizeAccessibility) {
      [NSLayoutConstraint deactivateConstraints:_standardConstraints];
      [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
    } else {
      [NSLayoutConstraint deactivateConstraints:_accessibilityConstraints];
      [NSLayoutConstraint activateConstraints:_standardConstraints];
    }
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  [self setIconImage:nil];
  [_switchView removeTarget:nil
                     action:nil
           forControlEvents:[_switchView allControlEvents]];
}

#pragma mark - UIAccessibility

- (CGPoint)accessibilityActivationPoint {
  // Center the activation point over the switch, so that double-tapping toggles
  // the switch.
  CGRect switchFrame =
      UIAccessibilityConvertFrameToScreenCoordinates(_switchView.frame, self);
  return CGPointMake(CGRectGetMidX(switchFrame), CGRectGetMidY(switchFrame));
}

- (NSString*)accessibilityHint {
  if (_switchView.enabled) {
    return _switchView.accessibilityHint;
  } else {
    return @"";
  }
}

- (NSString*)accessibilityLabel {
  return _textLabel.text;
}

- (NSString*)accessibilityValue {
  if (_switchView.on) {
    return l10n_util::GetNSString(IDS_IOS_SETTING_ON);
  } else {
    return l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  }
}

@end
