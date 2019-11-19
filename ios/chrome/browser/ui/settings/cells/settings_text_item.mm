// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kMargin = 16;
const CGFloat kMinimalHeight = 48;
}

@interface SettingsTextItem ()

// The maximum number of lines of the main text. Default is 1.
@property(nonatomic, assign) NSInteger numberOfTextLines;

// The font of the secondary text. Default is the regular Roboto font of size
// 14.
@property(nonatomic, null_resettable, copy) UIFont* detailTextFont;

// The color of the secondary text. Default is the 500 tint color of the grey
// palette.
@property(nonatomic, null_resettable, copy) UIColor* detailTextColor;

// The maximum number of lines of the secondary text. Default is 1.
@property(nonatomic, assign) NSInteger numberOfDetailTextLines;

@end

@implementation SettingsTextItem

@synthesize accessoryType = _accessoryType;
@synthesize text = _text;
@synthesize detailText = _detailText;
@synthesize textFont = _textFont;
@synthesize textColor = _textColor;
@synthesize numberOfTextLines = _numberOfTextLines;
@synthesize detailTextFont = _detailTextFont;
@synthesize detailTextColor = _detailTextColor;
@synthesize numberOfDetailTextLines = _numberOfDetailTextLines;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsTextCell class];
    _numberOfTextLines = 1;
    _numberOfDetailTextLines = 1;
  }
  return self;
}

- (UIFont*)textFont {
  if (!_textFont) {
    _textFont = [UIFont systemFontOfSize:kUIKitMainFontSize];
  }
  return _textFont;
}

- (UIColor*)textColor {
  if (!_textColor) {
    _textColor = [UIColor colorNamed:kTextPrimaryColor];
  }
  return _textColor;
}

- (UIFont*)detailTextFont {
  if (!_detailTextFont) {
    _detailTextFont = [UIFont systemFontOfSize:kUIKitMultilineDetailFontSize];
  }
  return _detailTextFont;
}

- (UIColor*)detailTextColor {
  if (!_detailTextColor) {
    _detailTextColor = [UIColor colorNamed:kTextSecondaryColor];
  }
  return _detailTextColor;
}

#pragma mark CollectionViewItem

- (void)configureCell:(SettingsTextCell*)cell {
  [super configureCell:cell];
  [cell cr_setAccessoryType:self.accessoryType];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  cell.isAccessibilityElement = YES;
  if ([self.accessibilityLabel length] != 0) {
    cell.accessibilityLabel = self.accessibilityLabel;
  } else {
    if (self.detailText.length == 0) {
      cell.accessibilityLabel = self.text;
    } else {
      cell.accessibilityLabel =
          [NSString stringWithFormat:@"%@, %@", self.text, self.detailText];
    }
  }

  // Styling.
  cell.textLabel.font = self.textFont;
  cell.textLabel.textColor = self.textColor;
  cell.textLabel.numberOfLines = self.numberOfTextLines;
  cell.detailTextLabel.font = self.detailTextFont;
  cell.detailTextLabel.textColor = self.detailTextColor;
  cell.detailTextLabel.numberOfLines = self.numberOfDetailTextLines;
}

@end

@implementation SettingsTextCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIView* containerView = [[UIView alloc] initWithFrame:CGRectZero];
    containerView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:containerView];

    _textLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [containerView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [containerView addSubview:_detailTextLabel];

    CGFloat margin = kMargin;

    [NSLayoutConstraint activateConstraints:@[
      // Total height.
      // The MDC specs ask for at least 48 pt.
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToConstant:kMinimalHeight],

      // Container.
      [containerView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:margin],
      [containerView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-margin],
      [containerView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      // Labels.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:containerView.leadingAnchor],
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:containerView.trailingAnchor],
      [_textLabel.topAnchor constraintEqualToAnchor:containerView.topAnchor],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.topAnchor],
      [_detailTextLabel.leadingAnchor
          constraintEqualToAnchor:_textLabel.leadingAnchor],
      [_detailTextLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:containerView.trailingAnchor],
      [_detailTextLabel.bottomAnchor
          constraintLessThanOrEqualToAnchor:containerView.bottomAnchor],
    ]];

    AddOptionalVerticalPadding(self.contentView, containerView, margin);
  }
  return self;
}

+ (CGFloat)heightForTitleLabel:(UILabel*)titleLabel
               detailTextLabel:(UILabel*)detailTextLabel
                         width:(CGFloat)width {
  CGSize sizeForLabel = CGSizeMake(width - 2 * kMargin, 500);

  CGFloat cellHeight = 2 * kMargin;
  cellHeight += [titleLabel sizeThatFits:sizeForLabel].height;
  cellHeight += [detailTextLabel sizeThatFits:sizeForLabel].height;

  return MAX(cellHeight, kMinimalHeight);
}

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];
  // Adjust the text and detailText label preferredMaxLayoutWidth when the
  // parent's width changes, for instance on screen rotation.
  CGFloat preferedMaxLayoutWidth =
      CGRectGetWidth(self.contentView.frame) - 2 * kMargin;
  _textLabel.preferredMaxLayoutWidth = preferedMaxLayoutWidth;
  _detailTextLabel.preferredMaxLayoutWidth = preferedMaxLayoutWidth;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
}

@end
