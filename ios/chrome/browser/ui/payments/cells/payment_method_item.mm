// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/cells/payment_method_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/payments/cells/accessibility_util.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Vertical spacing between the labels.
const CGFloat kVerticalSpacingBetweenLabels = 8;

// Padding used on the leading and trailing edges of the cell and between the
// favicon and labels.
const CGFloat kHorizontalPadding = 16;
}

@implementation PaymentMethodItem

@synthesize methodID = _methodID;
@synthesize methodDetail = _methodDetail;
@synthesize methodAddress = _methodAddress;
@synthesize notification = _notification;
@synthesize methodTypeIcon = _methodTypeIcon;
@synthesize reserveRoomForAccessoryType = _reserveRoomForAccessoryType;
@synthesize accessoryType = _accessoryType;
@synthesize complete = _complete;

#pragma mark CollectionViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PaymentMethodCell class];
  }
  return self;
}

- (void)configureCell:(PaymentMethodCell*)cell {
  [super configureCell:cell];
  cell.methodIDLabel.text = self.methodID;
  cell.methodDetailLabel.text = self.methodDetail;
  cell.methodAddressLabel.text = self.methodAddress;
  cell.notificationLabel.text = self.notification;
  cell.methodTypeIconView.image = self.methodTypeIcon;
  cell.reserveRoomForAccessoryType = self.reserveRoomForAccessoryType;
  [cell cr_setAccessoryType:self.accessoryType];
}

@end

@implementation PaymentMethodCell {
  UIStackView* _stackView;
  NSLayoutConstraint* _iconHeightConstraint;
  NSLayoutConstraint* _iconWidthConstraint;
}

@synthesize methodIDLabel = _methodIDLabel;
@synthesize methodDetailLabel = _methodDetailLabel;
@synthesize methodAddressLabel = _methodAddressLabel;
@synthesize notificationLabel = _notificationLabel;
@synthesize methodTypeIconView = _methodTypeIconView;
@synthesize reserveRoomForAccessoryType = _reserveRoomForAccessoryType;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;

    _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
    _stackView.axis = UILayoutConstraintAxisVertical;
    _stackView.layoutMarginsRelativeArrangement = YES;
    _stackView.layoutMargins =
        UIEdgeInsetsMake(kVerticalPadding, kHorizontalPadding, kVerticalPadding,
                         kHorizontalPadding);
    _stackView.alignment = UIStackViewAlignmentLeading;
    _stackView.spacing = kVerticalSpacingBetweenLabels;
    _stackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_stackView];

    // Method ID.
    _methodIDLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    SetUILabelScaledFont(_methodIDLabel, [MDCTypography body2Font]);
    _methodIDLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    [_stackView addArrangedSubview:_methodIDLabel];

    // Method detail.
    _methodDetailLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    SetUILabelScaledFont(_methodDetailLabel, [MDCTypography body1Font]);
    _methodDetailLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    [_stackView addArrangedSubview:_methodDetailLabel];

    // Method address.
    _methodAddressLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    SetUILabelScaledFont(_methodAddressLabel, [MDCTypography body1Font]);
    _methodAddressLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    [_stackView addArrangedSubview:_methodAddressLabel];

    // Notification label.
    _notificationLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    SetUILabelScaledFont(_notificationLabel, [MDCTypography body1Font]);
    _notificationLabel.textColor = [UIColor colorNamed:kBlueColor];
    [_stackView addArrangedSubview:_notificationLabel];

    // Method type icon.
    _methodTypeIconView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _methodTypeIconView.layer.borderColor =
        [UIColor colorWithWhite:0.9 alpha:1.0].CGColor;
    _methodTypeIconView.layer.borderWidth = 1.0;
    _methodTypeIconView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_methodTypeIconView];

    // Set up the icons size constraints. They are activated here and updated in
    // layoutSubviews.
    _iconHeightConstraint =
        [_methodTypeIconView.heightAnchor constraintEqualToConstant:0];
    _iconWidthConstraint =
        [_methodTypeIconView.widthAnchor constraintEqualToConstant:0];

    // Layout
    [NSLayoutConstraint activateConstraints:@[
      _iconHeightConstraint,
      _iconWidthConstraint,

      [_stackView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [_stackView.trailingAnchor
          constraintLessThanOrEqualToAnchor:_methodTypeIconView.leadingAnchor],
      [_stackView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor],
      [_stackView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
      [_methodTypeIconView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [_methodTypeIconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];
  }
  return self;
}

#pragma mark - UIView

- (void)layoutSubviews {
  _methodIDLabel.hidden = !_methodIDLabel.text;
  _methodDetailLabel.hidden = !_methodDetailLabel.text;
  _methodAddressLabel.hidden = !_methodAddressLabel.text;
  _notificationLabel.hidden = !_notificationLabel.text;

  // If reserving room for the accessory type, force the accessory type to
  // Checkmark for the duration of layout and later restore the real value.
  MDCCollectionViewCellAccessoryType realAccessoryType = self.accessoryType;
  if (_reserveRoomForAccessoryType) {
    self.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
  }

  // Set the size constraints of the icon view to the dimensions of the image.
  _iconHeightConstraint.constant = self.methodTypeIconView.image.size.height;
  _iconWidthConstraint.constant = self.methodTypeIconView.image.size.width;

  // Implement width adjustment per instructions in documentation for
  // +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
  [super layoutSubviews];

  // Adjust labels' preferredMaxLayoutWidth when the parent's width changes, for
  // instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.frame);
  CGFloat preferredMaxLayoutWidth =
      parentWidth - (_iconWidthConstraint.constant + (3 * kHorizontalPadding));
  _methodIDLabel.preferredMaxLayoutWidth = preferredMaxLayoutWidth;
  _methodDetailLabel.preferredMaxLayoutWidth = preferredMaxLayoutWidth;

  [super layoutSubviews];

  // Restore the real accessory type at the end of the layout.
  self.accessoryType = realAccessoryType;
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.methodIDLabel.text = nil;
  self.methodDetailLabel.text = nil;
  self.methodAddressLabel.text = nil;
  self.notificationLabel.text = nil;
  self.methodTypeIconView.image = nil;
  self.accessoryType = MDCCollectionViewCellAccessoryNone;
}

#pragma mark - Accessibility

- (NSString*)accessibilityLabel {
  AccessibilityLabelBuilder* builder = [[AccessibilityLabelBuilder alloc] init];
  [builder appendItem:self.methodIDLabel.text];
  [builder appendItem:self.methodDetailLabel.text];
  [builder appendItem:self.methodAddressLabel.text];
  [builder appendItem:self.notificationLabel.text];
  return [builder buildAccessibilityLabel];
}

@end
