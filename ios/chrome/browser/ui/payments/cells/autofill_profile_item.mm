// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/payments/cells/accessibility_util.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding of the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Padding of the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Spacing between the labels.
const CGFloat kVerticalSpacingBetweenLabels = 8;
}  // namespace

@interface AutofillProfileCell ()

// Sets dynamic font types if they are available (iOS 11+)
- (void)useScaledFont:(BOOL)useScaledFont;

@end

@implementation AutofillProfileItem

@synthesize name = _name;
@synthesize address = _address;
@synthesize phoneNumber = _phoneNumber;
@synthesize email = _email;
@synthesize notification = _notification;
@synthesize accessoryType = _accessoryType;
@synthesize complete = _complete;
@synthesize useScaledFont = _useScaledFont;

#pragma mark CollectionViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AutofillProfileCell class];
  }
  return self;
}

- (void)configureCell:(AutofillProfileCell*)cell {
  [super configureCell:cell];
  [cell cr_setAccessoryType:self.accessoryType];
  cell.nameLabel.text = self.name;
  cell.addressLabel.text = self.address;
  cell.phoneNumberLabel.text = self.phoneNumber;
  cell.emailLabel.text = self.email;
  cell.notificationLabel.text = self.notification;
  [cell useScaledFont:self.useScaledFont];
}

@end

@implementation AutofillProfileCell {
  UIStackView* _stackView;
}

@synthesize nameLabel = _nameLabel;
@synthesize addressLabel = _addressLabel;
@synthesize phoneNumberLabel = _phoneNumberLabel;
@synthesize emailLabel = _emailLabel;
@synthesize notificationLabel = _notificationLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    [self addSubviews];
    [self setDefaultViewStyling];
    [self setViewConstraints];
  }
  return self;
}

// Create and add subviews.
- (void)addSubviews {
  UIView* contentView = self.contentView;
  contentView.clipsToBounds = YES;

  _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  _stackView.axis = UILayoutConstraintAxisVertical;
  _stackView.layoutMarginsRelativeArrangement = YES;
  _stackView.layoutMargins =
      UIEdgeInsetsMake(kVerticalPadding, kHorizontalPadding, kVerticalPadding,
                       kHorizontalPadding);
  _stackView.alignment = UIStackViewAlignmentLeading;
  _stackView.spacing = kVerticalSpacingBetweenLabels;
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_stackView];

  _nameLabel = [[UILabel alloc] init];
  [_stackView addArrangedSubview:_nameLabel];

  _addressLabel = [[UILabel alloc] init];
  [_stackView addArrangedSubview:_addressLabel];

  _phoneNumberLabel = [[UILabel alloc] init];
  [_stackView addArrangedSubview:_phoneNumberLabel];

  _emailLabel = [[UILabel alloc] init];
  [_stackView addArrangedSubview:_emailLabel];

  _notificationLabel = [[UILabel alloc] init];
  [_stackView addArrangedSubview:_notificationLabel];
}

// Set default font and text colors for labels.
- (void)setDefaultViewStyling {
  [self useScaledFont:NO];
  _nameLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _nameLabel.numberOfLines = 0;
  _nameLabel.lineBreakMode = NSLineBreakByWordWrapping;

  _addressLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _addressLabel.numberOfLines = 0;
  _addressLabel.lineBreakMode = NSLineBreakByWordWrapping;

  _phoneNumberLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

  _emailLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

  _notificationLabel.textColor = [UIColor colorNamed:kBlueColor];
}

- (void)useScaledFont:(BOOL)useScaledFont {
  MaybeSetUILabelScaledFont(useScaledFont, _nameLabel,
                            [MDCTypography body2Font]);
  MaybeSetUILabelScaledFont(useScaledFont, _addressLabel,
                            [MDCTypography body1Font]);
  MaybeSetUILabelScaledFont(useScaledFont, _phoneNumberLabel,
                            [MDCTypography body1Font]);
  MaybeSetUILabelScaledFont(useScaledFont, _emailLabel,
                            [MDCTypography body1Font]);
  MaybeSetUILabelScaledFont(useScaledFont, _notificationLabel,
                            [MDCTypography body1Font]);
}

// Set constraints on subviews.
- (void)setViewConstraints {
  AddSameConstraints(self.contentView, _stackView);
}

#pragma mark - UIView

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  _nameLabel.hidden = !_nameLabel.text;
  _addressLabel.hidden = !_addressLabel.text;
  _phoneNumberLabel.hidden = !_phoneNumberLabel.text;
  _emailLabel.hidden = !_emailLabel.text;
  _notificationLabel.hidden = !_notificationLabel.text;

  // When the accessory type is None, the content view of the cell (and thus)
  // the labels inside it span larger than when there is a Checkmark accessory
  // type. That means that toggling the accessory type can induce a rewrapping
  // of the texts, which is not visually pleasing. To alleviate that issue
  // always lay out the cell as if there was a Checkmark accessory type.
  //
  // Force the accessory type to Checkmark for the duration of layout.
  MDCCollectionViewCellAccessoryType realAccessoryType = self.accessoryType;
  self.accessoryType = MDCCollectionViewCellAccessoryCheckmark;

  [super layoutSubviews];

  // Adjust preferredMaxLayoutWidth of _nameLabel and _addressLabel when the
  // parent's width changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.frame);
  CGFloat preferredMaxLayoutWidth = parentWidth - (2 * kHorizontalPadding);
  _nameLabel.preferredMaxLayoutWidth = preferredMaxLayoutWidth;
  _addressLabel.preferredMaxLayoutWidth = preferredMaxLayoutWidth;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];

  // Restore the real accessory type at the end of the layout.
  self.accessoryType = realAccessoryType;
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.nameLabel.text = nil;
  self.addressLabel.text = nil;
  self.phoneNumberLabel.text = nil;
  self.emailLabel.text = nil;
  self.notificationLabel.text = nil;
  self.accessoryType = MDCCollectionViewCellAccessoryNone;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  AccessibilityLabelBuilder* builder = [[AccessibilityLabelBuilder alloc] init];
  [builder appendItem:self.nameLabel.text];
  [builder appendItem:self.addressLabel.text];
  [builder appendItem:self.phoneNumberLabel.text];
  [builder appendItem:self.emailLabel.text];
  [builder appendItem:self.notificationLabel.text];
  return [builder buildAccessibilityLabel];
}

@end
