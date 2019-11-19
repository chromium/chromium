// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/cells/accepted_payment_methods_item.h"

#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding of the leading edge of the cell.
const CGFloat kLeadingPadding = 16;

// Padding of the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 12;

// Spacing between the icons.
const CGFloat kHorizontalSpacingBetweenIcons = 4.5;
}  // namespace

@implementation AcceptedPaymentMethodsItem

@synthesize message = _message;
@synthesize methodTypeIcons = _methodTypeIcons;

#pragma mark CollectionViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AcceptedPaymentMethodsCell class];
  }
  return self;
}

- (void)configureCell:(AcceptedPaymentMethodsCell*)cell {
  [super configureCell:cell];
  cell.messageLabel.text = self.message;

  NSMutableArray* methodTypeIconViews = [NSMutableArray array];
  for (UIImage* methodTypeIcon in self.methodTypeIcons) {
    UIImageView* methodTypeIconView =
        [[UIImageView alloc] initWithFrame:CGRectZero];
    methodTypeIconView.image = methodTypeIcon;
    methodTypeIconView.accessibilityLabel = methodTypeIcon.accessibilityLabel;
    [methodTypeIconViews addObject:methodTypeIconView];
  }
  cell.methodTypeIconViews = methodTypeIconViews;
}

@end

@implementation AcceptedPaymentMethodsCell {
  UIStackView* _stackView;
}

@synthesize messageLabel = _messageLabel;
@synthesize methodTypeIconViews = _methodTypeIconViews;

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

  _messageLabel = [[UILabel alloc] init];
  _messageLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_messageLabel];

  _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  _stackView.axis = UILayoutConstraintAxisHorizontal;
  _stackView.layoutMarginsRelativeArrangement = YES;
  _stackView.layoutMargins =
      UIEdgeInsetsMake(kVerticalPadding, kLeadingPadding, kVerticalPadding, 0);
  _stackView.spacing = kHorizontalSpacingBetweenIcons;
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_stackView];
}

// Set default font and text colors for labels.
- (void)setDefaultViewStyling {
  SetUILabelScaledFont(_messageLabel, [MDCTypography body2Font]);
  _messageLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _messageLabel.numberOfLines = 0;
  _messageLabel.lineBreakMode = NSLineBreakByWordWrapping;
}

// Set constraints on subviews.
- (void)setViewConstraints {
  [NSLayoutConstraint activateConstraints:@[
    [_messageLabel.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                            constant:kVerticalPadding],
    [_messageLabel.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kLeadingPadding],
    [_messageLabel.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor],
    [_messageLabel.bottomAnchor constraintEqualToAnchor:_stackView.topAnchor],
    [_stackView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor],
    [_stackView.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.contentView.trailingAnchor],
    [_stackView.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor],
  ]];
}

- (void)setMethodTypeIconViews:(NSArray<UIImageView*>*)methodTypeIconViews {
  // Remove the old UIImageViews.
  for (UIImageView* methodTypeIconView in _methodTypeIconViews)
    [methodTypeIconView removeFromSuperview];

  _methodTypeIconViews = methodTypeIconViews;

  // Add the new UIImageViews.
  for (UIImageView* methodTypeIconView in _methodTypeIconViews)
    [_stackView addArrangedSubview:methodTypeIconView];
}

#pragma mark - UIView

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust preferredMaxLayoutWidth of _messageLabel when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.frame);
  _messageLabel.preferredMaxLayoutWidth = parentWidth;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.messageLabel.text = nil;
  self.methodTypeIconViews = nil;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  NSMutableString* accessibilityLabel =
      [NSMutableString stringWithString:self.messageLabel.text];
  NSArray* iconsAccessibilityLabels =
      [self.methodTypeIconViews valueForKeyPath:@"accessibilityLabel"];
  NSString* concatenatedAccessibilityLabel =
      [iconsAccessibilityLabels componentsJoinedByString:@", "];
  if (concatenatedAccessibilityLabel.length) {
    [accessibilityLabel appendFormat:@", %@", concatenatedAccessibilityLabel];
  }
  return accessibilityLabel;
}

@end
