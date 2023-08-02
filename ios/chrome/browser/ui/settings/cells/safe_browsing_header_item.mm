// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/safe_browsing_header_item.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {
// Icon image size is forced to 24pt and doesn't follow standard icon image size
// of 30pt since the icon is in a header and should be smaller.
const CGFloat kSafeBrowsingHeaderIconImageSize = 24;
}  // namespace

@implementation SafeBrowsingHeaderItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SafeBrowsingHeaderView class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureHeaderFooterView:(SafeBrowsingHeaderView*)header
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:header withStyler:styler];
  header.textLabel.text = self.text;
  header.textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  header.image = self.image;

  if (self.attributedText) {
    header.textLabel.attributedText = self.attributedText;
  }

  if (self.imageViewTintColor) {
    [header setImageViewTintColor:self.imageViewTintColor];
  }
}

@end

@interface SafeBrowsingHeaderView ()

// Image view for the cell.
@property(nonatomic, strong) UIImageView* imageView;

// Constraint used for leading text constraint without `imageView`.
@property(nonatomic, strong) NSLayoutConstraint* textNoImageConstraint;

// Constraint used for leading text constraint with `imageView` showing.
@property(nonatomic, strong) NSLayoutConstraint* textWithImageConstraint;

@end

@implementation SafeBrowsingHeaderView

@synthesize textLabel = _textLabel;

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    [self addSubviews];
    [self setViewConstraints];
  }
  return self;
}

// Creates and adds subviews.
- (void)addSubviews {
  UIView* contentView = self.contentView;

  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  _imageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  [contentView addSubview:_imageView];

  _textLabel = [[UILabel alloc] init];
  _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _textLabel.numberOfLines = 0;
  _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _textLabel.adjustsFontForContentSizeCategory = YES;

  [contentView addSubview:_textLabel];
}

// Sets constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  _textNoImageConstraint = [_textLabel.leadingAnchor
      constraintEqualToAnchor:contentView.leadingAnchor
                     constant:kTableViewHorizontalSpacing];

  _textWithImageConstraint = [_textLabel.leadingAnchor
      constraintEqualToAnchor:_imageView.trailingAnchor
                     constant:kTableViewImagePadding];

  [NSLayoutConstraint activateConstraints:@[
    [_imageView.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                             constant:HorizontalPadding()],
    [_imageView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [_imageView.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:
                                        kTableViewOneLabelCellVerticalSpacing],
    [_imageView.widthAnchor
        constraintEqualToConstant:kSafeBrowsingHeaderIconImageSize],
    [_imageView.heightAnchor constraintEqualToAnchor:_imageView.widthAnchor],
    [contentView.trailingAnchor
        constraintEqualToAnchor:_textLabel.trailingAnchor
                       constant:-HorizontalPadding()],
    [contentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:_imageView.bottomAnchor
                                    constant:kTableViewVerticalSpacing],
    [_textLabel.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [_textLabel.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:kTableViewVerticalSpacing],
    [contentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:_textLabel.bottomAnchor
                                    constant:kTableViewVerticalSpacing],
  ]];
}

- (void)setImage:(UIImage*)image {
  BOOL hidden = !image;
  self.imageView.image = image;
  self.imageView.hidden = hidden;
  // Update the leading text constraint based on `image` being provided.
  if (hidden) {
    self.textWithImageConstraint.active = NO;
    self.textNoImageConstraint.active = YES;
  } else {
    self.textNoImageConstraint.active = NO;
    self.textWithImageConstraint.active = YES;
  }
}

- (UIImage*)image {
  return self.imageView.image;
}

- (void)setImageViewTintColor:(UIColor*)color {
  self.imageView.tintColor = color;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.imageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  if (!self.textLabel.text) {
    return self.detailTextLabel.text;
  }

  if (self.detailTextLabel.text) {
    return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                      self.detailTextLabel.text];
  }

  return self.textLabel.text;
}

@end
