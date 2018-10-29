// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/account_signin_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Padding used between the image and text.
const CGFloat kHorizontalPaddingBetweenImageAndText = 10;

// Image fixed horizontal size.
const CGFloat kHorizontalImageFixedSize = 40;

// Font size for the main text.
const CGFloat kMainTextFontSize = 14;

// Font size for detail text.
const CGFloat kDetailTextFontSize = 14;
}

@implementation AccountSignInItem

@synthesize image = _image;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AccountSignInCell class];
    self.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  return self;
}

#pragma mark - CollectionViewItem

- (void)configureCell:(AccountSignInCell*)cell {
  [super configureCell:cell];
  cell.textLabel.text =
      l10n_util::GetNSString(IDS_IOS_SIGN_IN_TO_CHROME_SETTING_TITLE);
  cell.detailTextLabel.text =
      l10n_util::GetNSString(IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SUBTITLE);
  cell.imageView.image = self.image;
}

@end

@implementation AccountSignInCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
@synthesize imageView = _imageView;

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

  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_imageView];

  _textLabel = [[UILabel alloc] init];
  _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_textLabel];

  _detailTextLabel = [[UILabel alloc] init];
  _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [_detailTextLabel setNumberOfLines:3];
  [contentView addSubview:_detailTextLabel];
}

- (void)setDefaultViewStyling {
  _imageView.contentMode = UIViewContentModeCenter;
  _imageView.layer.masksToBounds = YES;
  _imageView.contentMode = UIViewContentModeScaleAspectFit;

  _textLabel.font = [UIFont systemFontOfSize:kMainTextFontSize];
  _textLabel.textColor = UIColorFromRGB(kUIKitMainTextColor);

  _detailTextLabel.font = [UIFont systemFontOfSize:kDetailTextFontSize];
  _detailTextLabel.textColor = UIColorFromRGB(kUIKitMultilineDetailTextColor);
}

- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  // This guide is used to center the two leading textLabels.
  UILayoutGuide* verticalCenteringGuide = [[UILayoutGuide alloc] init];
  [contentView addLayoutGuide:verticalCenteringGuide];

  [NSLayoutConstraint activateConstraints:@[
    // Set leading anchors.
    [_imageView.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                             constant:kHorizontalPadding],
    [_detailTextLabel.leadingAnchor
        constraintEqualToAnchor:_textLabel.leadingAnchor],
    [_textLabel.leadingAnchor
        constraintEqualToAnchor:_imageView.trailingAnchor
                       constant:kHorizontalPaddingBetweenImageAndText],

    // Fix image widths.
    [_imageView.widthAnchor
        constraintEqualToConstant:kHorizontalImageFixedSize],

    // Set vertical anchors. This approach assumes the cell height is set by
    // the view controller. Contents are pinned to centerY, rather than pushing
    // against the top/bottom boundaries.
    [_imageView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [_textLabel.topAnchor
        constraintEqualToAnchor:verticalCenteringGuide.topAnchor],
    [_textLabel.bottomAnchor
        constraintEqualToAnchor:_detailTextLabel.topAnchor],
    [_detailTextLabel.bottomAnchor
        constraintEqualToAnchor:verticalCenteringGuide.bottomAnchor],
    [verticalCenteringGuide.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    // Set trailing anchors.
    [_detailTextLabel.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kHorizontalPadding],
    [_textLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:contentView.trailingAnchor
                                 constant:-kHorizontalPadding],
  ]];

  // This is needed so the image doesn't get pushed out if both text and detail
  // are long.
  [_textLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_detailTextLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  _imageView.image = nil;
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  return l10n_util::GetNSString(IDS_IOS_SIGN_IN_TO_CHROME_SETTING_TITLE);
}

- (NSString*)accessibilityValue {
  return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                    self.detailTextLabel.text];
}

@end
