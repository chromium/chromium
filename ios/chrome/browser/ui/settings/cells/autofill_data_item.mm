// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/autofill_data_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;
// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;
}

@implementation AutofillDataItem

@synthesize deletable = _deletable;
@synthesize GUID = _GUID;
@synthesize text = _text;
@synthesize leadingDetailText = _leadingDetailText;
@synthesize trailingDetailText = _trailingDetailText;
@synthesize accessoryType = _accessoryType;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AutofillDataCell class];
  }
  return self;
}

#pragma mark - CollectionViewItem

- (void)configureCell:(AutofillDataCell*)cell {
  [super configureCell:cell];
  cell.textLabel.text = self.text;
  cell.leadingDetailTextLabel.text = self.leadingDetailText;
  cell.trailingDetailTextLabel.text = self.trailingDetailText;
  [cell cr_setAccessoryType:self.accessoryType];
}

@end

@implementation AutofillDataCell {
  NSLayoutConstraint* _textLabelWidthConstraint;
}

@synthesize textLabel = _textLabel;
@synthesize leadingDetailTextLabel = _leadingDetailTextLabel;
@synthesize trailingDetailTextLabel = _trailingDetailTextLabel;

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

// Creates and adds subviews.
- (void)addSubviews {
  UIView* contentView = self.contentView;

  _textLabel = [[UILabel alloc] init];
  _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_textLabel];

  _leadingDetailTextLabel = [[UILabel alloc] init];
  _leadingDetailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_leadingDetailTextLabel];

  _trailingDetailTextLabel = [[UILabel alloc] init];
  _trailingDetailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_trailingDetailTextLabel];
}

// Sets default font and text colors for labels.
- (void)setDefaultViewStyling {
  _textLabel.numberOfLines = 0;
  _textLabel.lineBreakMode = NSLineBreakByWordWrapping;
  _textLabel.font = [UIFont systemFontOfSize:kUIKitMainFontSize];
  _textLabel.textColor = UIColorFromRGB(kUIKitMainTextColor);

  _leadingDetailTextLabel.numberOfLines = 0;
  _leadingDetailTextLabel.lineBreakMode = NSLineBreakByWordWrapping;
  _leadingDetailTextLabel.font =
      [UIFont systemFontOfSize:kUIKitMultilineDetailFontSize];
  _leadingDetailTextLabel.textColor =
      UIColorFromRGB(kUIKitMultilineDetailTextColor);

  _trailingDetailTextLabel.font =
      [UIFont systemFontOfSize:kUIKitDetailFontSize];
  _trailingDetailTextLabel.textColor = UIColorFromRGB(kUIKitDetailTextColor);
}

// Sets constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  // Set up the width constraint for the text label. It is activated here
  // and updated in layoutSubviews.
  _textLabelWidthConstraint =
      [_textLabel.widthAnchor constraintEqualToConstant:0];

  [NSLayoutConstraint activateConstraints:@[
    // Set horizontal anchors.
    [_textLabel.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                             constant:kHorizontalPadding],
    [_leadingDetailTextLabel.leadingAnchor
        constraintEqualToAnchor:_textLabel.leadingAnchor],
    [_trailingDetailTextLabel.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kHorizontalPadding],

    // Set width anchors.
    [_leadingDetailTextLabel.widthAnchor
        constraintEqualToAnchor:_textLabel.widthAnchor],
    _textLabelWidthConstraint,

    // Set vertical anchors.
    [_leadingDetailTextLabel.topAnchor
        constraintEqualToAnchor:_textLabel.bottomAnchor],
    [_trailingDetailTextLabel.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];

  AddOptionalVerticalPadding(contentView, _textLabel, _leadingDetailTextLabel,
                             kVerticalPadding);
}

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Size the trailing detail label to determine how much width it wants.
  [self.trailingDetailTextLabel sizeToFit];

  // Update the text label's width constraint.
  CGFloat availableWidth =
      CGRectGetWidth(self.contentView.bounds) - (3 * kHorizontalPadding);
  CGFloat trailingDetailLabelWidth =
      CGRectGetWidth(self.trailingDetailTextLabel.frame);
  _textLabelWidthConstraint.constant =
      availableWidth - trailingDetailLabelWidth;

  [super layoutSubviews];
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.leadingDetailTextLabel.text = nil;
  self.trailingDetailTextLabel.text = nil;
  self.accessoryType = MDCCollectionViewCellAccessoryNone;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  if (self.trailingDetailTextLabel.text) {
    return [NSString stringWithFormat:@"%@, %@, %@", self.textLabel.text,
                                      self.leadingDetailTextLabel.text,
                                      self.trailingDetailTextLabel.text];
  }
  return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                    self.leadingDetailTextLabel.text];
}

@end
