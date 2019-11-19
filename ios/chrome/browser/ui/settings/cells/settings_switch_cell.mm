// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"

#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used between the icon and the text labels.
const CGFloat kIconTrailingPadding = 12;

// Size of the icon image.
const CGFloat kIconImageSize = 28;
}  // namespace

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

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;

    _iconImageView = [[UIImageView alloc] init];
    _iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconImageView.hidden = YES;
    [self.contentView addSubview:_iconImageView];

    UILayoutGuide* textLayoutGuide = [[UILayoutGuide alloc] init];
    [self.contentView addLayoutGuide:textLayoutGuide];

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = UIColor.cr_labelColor;
    _textLabel.numberOfLines = 0;
    [self.contentView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.textColor = UIColor.cr_secondaryLabelColor;
    _detailTextLabel.numberOfLines = 0;
    [self.contentView addSubview:_detailTextLabel];

    _switchView = [[UISwitch alloc] initWithFrame:CGRectZero];
    _switchView.translatesAutoresizingMaskIntoConstraints = NO;
    [_switchView
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    _switchView.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_TOGGLE_SETTING_SWITCH_ACCESSIBILITY_HINT);
    [self.contentView addSubview:_switchView];

    // Set up the constraints assuming that the icon image is hidden.
    _iconVisibleConstraint = [textLayoutGuide.leadingAnchor
        constraintEqualToAnchor:_iconImageView.trailingAnchor
                       constant:kIconTrailingPadding];
    _iconHiddenConstraint = [textLayoutGuide.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing];

    _standardConstraints = @[
      [_switchView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [textLayoutGuide.trailingAnchor
          constraintLessThanOrEqualToAnchor:_switchView.leadingAnchor
                                   constant:-kTableViewHorizontalSpacing],
      [textLayoutGuide.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      [_switchView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
    ];
    _accessibilityConstraints = @[
      [_switchView.topAnchor
          constraintEqualToAnchor:textLayoutGuide.bottomAnchor
                         constant:kTableViewLargeVerticalSpacing],
      [_switchView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_switchView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kTableViewLargeVerticalSpacing],
      [textLayoutGuide.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.trailingAnchor
                                   constant:-kTableViewHorizontalSpacing],
    ];

    [NSLayoutConstraint activateConstraints:@[
      [_iconImageView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_iconImageView.widthAnchor constraintEqualToConstant:kIconImageSize],
      [_iconImageView.heightAnchor constraintEqualToConstant:kIconImageSize],

      [_iconImageView.centerYAnchor
          constraintEqualToAnchor:textLayoutGuide.centerYAnchor],

      _iconHiddenConstraint,

      [textLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_textLabel.leadingAnchor],
      [textLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_detailTextLabel.leadingAnchor],
      [textLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_textLabel.trailingAnchor],
      [textLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_detailTextLabel.trailingAnchor],
      [textLayoutGuide.topAnchor constraintEqualToAnchor:_textLabel.topAnchor],
      [textLayoutGuide.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.bottomAnchor],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.topAnchor],
    ]];

    if (UIContentSizeCategoryIsAccessibilityCategory(
            self.traitCollection.preferredContentSizeCategory)) {
      [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
    } else {
      [NSLayoutConstraint activateConstraints:_standardConstraints];
    }

    AddOptionalVerticalPadding(self.contentView, textLayoutGuide,
                               kTableViewOneLabelCellVerticalSpacing);
  }
  return self;
}

+ (UIColor*)defaultTextColorForState:(UIControlState)state {
  return (state & UIControlStateDisabled) ? UIColor.cr_secondaryLabelColor
                                          : UIColor.cr_labelColor;
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
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (UIContentSizeCategoryIsAccessibilityCategory(
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

  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
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
  if (!self.detailTextLabel.text)
    return self.textLabel.text;
  return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                    self.detailTextLabel.text];
}

- (NSString*)accessibilityValue {
  if (_switchView.on) {
    return l10n_util::GetNSString(IDS_IOS_SETTING_ON);
  } else {
    return l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  }
}

- (UIAccessibilityTraits)accessibilityTraits {
  UIAccessibilityTraits accessibilityTraits = super.accessibilityTraits;
  if (!self.switchView.isEnabled) {
    accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  return accessibilityTraits;
}

@end
