// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"

#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Image height and width.
const CGFloat kImageSize = 40;

// Padding used between the image and text.
const CGFloat kHorizontalImagePadding = 10;

}  // namespace

@interface SettingsImageDetailTextCell ()

// Container view which contains |self.textLabel| and |self.detailTextLabel|.
@property(nonatomic, strong) UIStackView* textStackView;

@end

#pragma mark - SettingsImageDetailTextItem

@implementation SettingsImageDetailTextItem

@synthesize image = _image;
@synthesize text = _text;
@synthesize detailText = _detailText;
@synthesize commandID = _commandID;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsImageDetailTextCell class];
  }
  return self;
}

- (void)configureCell:(SettingsImageDetailTextCell*)cell {
  [super configureCell:cell];
  cell.isAccessibilityElement = YES;
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  cell.imageView.image = self.image;
}

@end

#pragma mark - SettingsImageDetailTextCell

@implementation SettingsImageDetailTextCell

@synthesize imageView = _imageView;
@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
@synthesize textStackView = _textStackView;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIView* contentView = self.contentView;

    _imageView = [[UIImageView alloc] init];
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_imageView];

    _textLabel = [[UILabel alloc] init];
    _textLabel.numberOfLines = 0;
    _textLabel.font = [UIFont systemFontOfSize:kUIKitMainFontSize];
    _textLabel.textColor = UIColorFromRGB(kUIKitMainTextColor);

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.numberOfLines = 0;
    _detailTextLabel.font =
        [UIFont systemFontOfSize:kUIKitMultilineDetailFontSize];
    _detailTextLabel.textColor = UIColorFromRGB(kUIKitMultilineDetailTextColor);

    _textStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textLabel, _detailTextLabel ]];
    _textStackView.axis = UILayoutConstraintAxisVertical;
    _textStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_textStackView];

    [NSLayoutConstraint activateConstraints:@[
      // Horizontal contraints for |_imageView| and |_textStackView|.
      [_imageView.heightAnchor constraintEqualToConstant:kImageSize],
      [_imageView.widthAnchor constraintEqualToConstant:kImageSize],
      [_imageView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_textStackView.leadingAnchor
          constraintEqualToAnchor:_imageView.trailingAnchor
                         constant:kHorizontalImagePadding],
      [contentView.trailingAnchor
          constraintEqualToAnchor:_textStackView.trailingAnchor
                         constant:kHorizontalPadding],
      // Vertical contraints for |_imageView| and |_textStackView|.
      [_imageView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_imageView.topAnchor
          constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                      constant:kVerticalPadding],
      [contentView.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:_imageView.bottomAnchor
                                      constant:kVerticalPadding],
      [_textStackView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_textStackView.topAnchor
          constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                      constant:kVerticalPadding],
      [contentView.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:_textStackView.bottomAnchor
                                      constant:kVerticalPadding],
    ]];
  }
  return self;
}

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:]."
- (void)layoutSubviews {
  [super layoutSubviews];
  // Adjust the text and detailText label preferredMaxLayoutWidth when the
  // parent's width changes, for instance on screen rotation.
  CGFloat prefferedMaxLayoutWidth = CGRectGetWidth(self.contentView.frame) -
                                    CGRectGetWidth(self.imageView.frame) -
                                    2 * kHorizontalPadding -
                                    kHorizontalImagePadding;
  self.textLabel.preferredMaxLayoutWidth = prefferedMaxLayoutWidth;
  self.detailTextLabel.preferredMaxLayoutWidth = prefferedMaxLayoutWidth;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  if (self.detailTextLabel.text) {
    return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                      self.detailTextLabel.text];
  }
  return self.textLabel.text;
}

@end
