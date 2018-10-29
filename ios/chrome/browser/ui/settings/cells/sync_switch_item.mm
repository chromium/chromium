// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"

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

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Padding used between the switch and text.
const CGFloat kHorizontalSwitchPadding = 10;
}  // namespace

#pragma mark - SyncSwitchItem

@implementation SyncSwitchItem

@synthesize text = _text;
@synthesize detailText = _detailText;
@synthesize on = _on;
@synthesize enabled = _enabled;
@synthesize dataType = _dataType;
@synthesize commandID = _commandID;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SyncSwitchCell class];
    self.enabled = YES;
  }
  return self;
}

- (void)configureCell:(SyncSwitchCell*)cell {
  [super configureCell:cell];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  cell.switchView.enabled = self.isEnabled;
  cell.switchView.on = self.isOn;
  cell.textLabel.textColor =
      [SyncSwitchCell defaultTextColorForState:cell.switchView.state];
  if (self.isEnabled) {
    cell.textLabel.textColor = [[MDCPalette greyPalette] tint900];
    cell.switchView.enabled = YES;
    cell.userInteractionEnabled = YES;
  } else {
    cell.textLabel.textColor = [[MDCPalette greyPalette] tint500];
    cell.switchView.enabled = NO;
    cell.userInteractionEnabled = NO;
  }
}

@end

@implementation SyncSwitchCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
@synthesize switchView = _switchView;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont systemFontOfSize:kUIKitMainFontSize];
    _textLabel.textColor = UIColorFromRGB(kUIKitMainTextColor);
    _textLabel.numberOfLines = 0;
    [self.contentView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font =
        [UIFont systemFontOfSize:kUIKitMultilineDetailFontSize];
    _detailTextLabel.textColor = UIColorFromRGB(kUIKitMultilineDetailTextColor);
    _detailTextLabel.numberOfLines = 0;
    [self.contentView addSubview:_detailTextLabel];

    _switchView = [[UISwitch alloc] initWithFrame:CGRectZero];
    _switchView.translatesAutoresizingMaskIntoConstraints = NO;
    _switchView.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_TOGGLE_SETTING_SWITCH_ACCESSIBILITY_HINT);
    _switchView.onTintColor = UIColorFromRGB(kUIKitSwitchTintColor);
    [self.contentView addSubview:_switchView];

    [self setConstraints];
  }

  return self;
}

- (void)setConstraints {
  UIView* contentView = self.contentView;

  [NSLayoutConstraint activateConstraints:@[
    [_textLabel.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                             constant:kHorizontalPadding],
    [_detailTextLabel.leadingAnchor
        constraintEqualToAnchor:_textLabel.leadingAnchor],
    [_switchView.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kHorizontalPadding],
    [_textLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_switchView.leadingAnchor
                                 constant:-kHorizontalSwitchPadding],
    [_detailTextLabel.trailingAnchor
        constraintEqualToAnchor:_textLabel.trailingAnchor],
    [_textLabel.bottomAnchor
        constraintEqualToAnchor:_detailTextLabel.topAnchor],
    [_switchView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];
  AddOptionalVerticalPadding(contentView, _textLabel, _detailTextLabel,
                             kVerticalPadding);
}

+ (UIColor*)defaultTextColorForState:(UIControlState)state {
  MDCPalette* grey = [MDCPalette greyPalette];
  return (state & UIControlStateDisabled) ? grey.tint500 : grey.tint900;
}

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];
  // Adjust the text and detailText label preferredMaxLayoutWidth when the
  // parent's width
  // changes, for instance on screen rotation.
  CGFloat prefferedMaxLayoutWidth = CGRectGetWidth(self.contentView.frame) -
                                    CGRectGetWidth(_switchView.frame) -
                                    2 * kHorizontalPadding -
                                    kHorizontalSwitchPadding;
  _textLabel.preferredMaxLayoutWidth = prefferedMaxLayoutWidth;
  _detailTextLabel.preferredMaxLayoutWidth = prefferedMaxLayoutWidth;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.tag = 0;
  self.textLabel.textColor = [[MDCPalette greyPalette] tint900];
  [self.switchView setEnabled:YES];
  [self setUserInteractionEnabled:YES];

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
  if (_detailTextLabel.text) {
    return [NSString
        stringWithFormat:@"%@, %@", _textLabel.text, _detailTextLabel.text];
  }
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
