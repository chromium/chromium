// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/ui/cells/plus_address_suggestion_label_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Height and Width of the trailing and the leading image views.
const CGFloat kImageViewSize = 20.0;

// Padding used between leading image view and label.
const CGFloat kLeadingImageViewPadding = 20.0;
}  // namespace

@interface PlusAddressSuggestionLabelCell () {
  // The trailing image view that acts as a button.
  UIButton* _trailingButtonView;

  // Image view for the leading image.
  UIImageView* _leadingImageView;
}

@end

@implementation PlusAddressSuggestionLabelCell

@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    UIView* contentView = self.contentView;

    // Attributes of row contents in order or appearance (if present).

    // `_leadingImageView` attributes
    _leadingImageView = [[UIImageView alloc] init];
    _leadingImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _leadingImageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
    _leadingImageView.contentMode = UIViewContentModeCenter;
    [contentView addSubview:_leadingImageView];

    // Text attributes.
    // `_textLabel` attributes.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [contentView addSubview:_textLabel];

    // `_trailingButtonView` attributes.
    _trailingButtonView = [UIButton buttonWithType:UIButtonTypeCustom];
    _trailingButtonView.translatesAutoresizingMaskIntoConstraints = NO;
    _trailingButtonView.hidden = YES;
    [_trailingButtonView addTarget:self
                            action:@selector(trailingButtonTapped:)
                  forControlEvents:UIControlEventTouchUpInside];
    [contentView addSubview:_trailingButtonView];

    // Constraints.
    [NSLayoutConstraint activateConstraints:@[
      // Constraints for `_trailingButtonView`.
      [_trailingButtonView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_trailingButtonView.widthAnchor
          constraintEqualToConstant:kImageViewSize],
      [_trailingButtonView.heightAnchor
          constraintEqualToAnchor:_trailingButtonView.widthAnchor],
      [_trailingButtonView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      // Constraints for `_leadingImageView`.
      [_leadingImageView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_leadingImageView.widthAnchor constraintEqualToConstant:kImageViewSize],
      [_leadingImageView.heightAnchor
          constraintEqualToAnchor:_leadingImageView.widthAnchor],
      [_leadingImageView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      // Constraints for `_textLabel`.
      [_textLabel.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:_leadingImageView.trailingAnchor
                         constant:kLeadingImageViewPadding],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:_trailingButtonView.leadingAnchor
                         constant:-kTableViewImagePadding],
    ]];
  }
  return self;
}

- (void)setTrailingButtonImage:(UIImage*)trailingButtonImage
                 withTintColor:(UIColor*)trailingImageColor {
  _trailingButtonView.hidden = NO;
  _trailingButtonView.tintColor = trailingImageColor;
  [_trailingButtonView setImage:trailingButtonImage
                       forState:UIControlStateNormal];
}

- (void)setLeadingIconImage:(UIImage*)image withTintColor:(UIColor*)tintColor {
  _leadingImageView.image = image;
  _leadingImageView.tintColor = tintColor;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  self.textLabel.text = nil;
  self.accessibilityCustomActions = nil;
  [self setTrailingButtonImage:nil withTintColor:nil];
  [self setLeadingIconImage:nil withTintColor:nil];
  _trailingButtonView.hidden = YES;
}

#pragma mark - Private

- (void)trailingButtonTapped:(UIButton*)sender {
  [self.delegate didTapTrailingButton];
}

@end
