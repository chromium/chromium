// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/password_details_item.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/ui_util/constraints_ui_util.h"
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
}  // namespace

@implementation PasswordDetailsItem

@synthesize text = _text;
@synthesize showingText = _showingText;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PasswordDetailsCell class];
  }
  return self;
}

- (void)configureCell:(PasswordDetailsCell*)cell {
  [super configureCell:cell];
  if (self.showingText) {
    cell.textLabel.text = self.text;
    cell.textLabel.accessibilityLabel = self.text;
  } else {
    NSString* obscuredText = [@"" stringByPaddingToLength:[self.text length]
                                               withString:@"•"
                                          startingAtIndex:0];
    cell.textLabel.text = obscuredText;
    cell.textLabel.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL);
  }
}

@end

@implementation PasswordDetailsCell

@synthesize textLabel = _textLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont systemFontOfSize:kUIKitMainFontSize];
    _textLabel.textColor = UIColorFromRGB(kUIKitMainTextColor);
    _textLabel.numberOfLines = 0;
    [contentView addSubview:_textLabel];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],
    ]];
    AddOptionalVerticalPadding(contentView, _textLabel, kVerticalPadding);
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.textLabel.accessibilityLabel = nil;
}

// Implements -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);
  self.textLabel.preferredMaxLayoutWidth = parentWidth - 2 * kHorizontalPadding;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

@end
