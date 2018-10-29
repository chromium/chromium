// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/passphrase_error_item.h"

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;
}  // namespace

@implementation PassphraseErrorItem

@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PassphraseErrorCell class];
  }
  return self;
}

- (void)configureCell:(PassphraseErrorCell*)cell {
  [super configureCell:cell];
  cell.textLabel.text = self.text;
}

@end

@interface PassphraseErrorCell ()
@property(nonatomic, readonly, strong) UIImageView* errorImageView;
@end

@implementation PassphraseErrorCell

@synthesize textLabel = _textLabel;
@synthesize errorImageView = _errorImageView;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont systemFontOfSize:kUIKitMainFontSize];
    _textLabel.textColor = [[MDCPalette cr_redPalette] tint500];
    [contentView addSubview:_textLabel];

    _errorImageView = [[UIImageView alloc] init];
    _errorImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _errorImageView.image = [UIImage imageNamed:@"encryption_error"];
    [contentView addSubview:_errorImageView];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_errorImageView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:_errorImageView.trailingAnchor
                         constant:kHorizontalPadding],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [_errorImageView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_textLabel.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
    ]];

    [_errorImageView
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisHorizontal];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
}

@end
