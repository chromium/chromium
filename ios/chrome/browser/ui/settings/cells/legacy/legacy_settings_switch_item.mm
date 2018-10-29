// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_switch_item.h"

#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Padding used between the icon and the text labels.
const CGFloat kIconTrailingPadding = 12;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Size of the icon image.
const CGFloat kIconImageSize = 28;
}  // namespace

@implementation LegacySettingsSwitchItem

@synthesize enabled = _enabled;
@synthesize iconImageName = _iconImageName;
@synthesize on = _on;
@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacySettingsSwitchCell class];
    self.enabled = YES;
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(LegacySettingsSwitchCell*)cell {
  [super configureCell:cell];
  cell.textLabel.text = self.text;
  cell.switchView.enabled = self.isEnabled;
  cell.switchView.on = self.isOn;
  cell.textLabel.textColor =
      [LegacySettingsSwitchCell defaultTextColorForState:cell.switchView.state];

  // Update the icon image, if one is present.
  UIImage* iconImage = nil;
  if ([self.iconImageName length]) {
    iconImage = [UIImage imageNamed:self.iconImageName];
  }
  [cell setIconImage:iconImage];
}

@end

@interface LegacySettingsSwitchCell ()

// The image view for the leading icon.
@property(nonatomic, readonly, strong) UIImageView* iconImageView;

// Constraints that are used when the iconImageView is visible and hidden.
@property(nonatomic, strong) NSLayoutConstraint* iconVisibleConstraint;
@property(nonatomic, strong) NSLayoutConstraint* iconHiddenConstraint;

@end

@implementation LegacySettingsSwitchCell

@synthesize iconHiddenConstraint = _iconHiddenConstraint;
@synthesize iconImageView = _iconImageView;
@synthesize iconVisibleConstraint = _iconVisibleConstraint;
@synthesize switchView = _switchView;
@synthesize textLabel = _textLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;

    _iconImageView = [[UIImageView alloc] init];
    _iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconImageView.hidden = YES;
    [self.contentView addSubview:_iconImageView];

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont systemFontOfSize:kUIKitMainFontSize];
    _textLabel.textColor = UIColorFromRGB(kUIKitMainTextColor);
    _textLabel.numberOfLines = 0;
    [self.contentView addSubview:_textLabel];

    _switchView = [[UISwitch alloc] initWithFrame:CGRectZero];
    _switchView.translatesAutoresizingMaskIntoConstraints = NO;
    _switchView.onTintColor = UIColorFromRGB(kUIKitSwitchTintColor);
    _switchView.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_TOGGLE_SETTING_SWITCH_ACCESSIBILITY_HINT);
    [self.contentView addSubview:_switchView];

    // Set up the constraints assuming that the icon image is hidden..
    _iconVisibleConstraint = [_textLabel.leadingAnchor
        constraintEqualToAnchor:_iconImageView.trailingAnchor
                       constant:kIconTrailingPadding];
    _iconHiddenConstraint = [_textLabel.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kHorizontalPadding];

    [NSLayoutConstraint activateConstraints:@[
      [_iconImageView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_iconImageView.widthAnchor constraintEqualToConstant:kIconImageSize],
      [_iconImageView.heightAnchor constraintEqualToConstant:kIconImageSize],

      [_switchView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_switchView.leadingAnchor
                                   constant:-kHorizontalPadding],

      [_iconImageView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_textLabel.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_switchView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      _iconHiddenConstraint,
    ]];

    AddOptionalVerticalPadding(self.contentView, _textLabel, kVerticalPadding);
  }
  return self;
}

+ (UIColor*)defaultTextColorForState:(UIControlState)state {
  return (state & UIControlStateDisabled)
             ? UIColorFromRGB(kUIKitDetailTextColor)
             : UIColorFromRGB(kUIKitMainTextColor);
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

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];
  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  _textLabel.preferredMaxLayoutWidth = CGRectGetWidth(self.contentView.frame) -
                                       CGRectGetWidth(_switchView.frame) -
                                       3 * kHorizontalPadding;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

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
  if (_switchView.isEnabled) {
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
