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

// Size of activity indicator replacing image view when active.
const CGFloat kIndicatorSize = 16.0;
}  // namespace

@interface PlusAddressSuggestionLabelCell () {
  // The trailing image view that acts as a button.
  UIButton* _trailingButtonView;

  // Image view for the leading image.
  UIImageView* _leadingImageView;

  // Activity Indicator view. Either the `_leadingImageView` is shown or this.
  UIActivityIndicatorView* _activityIndicatorView;
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

    // `_activityIndicatorView` attributes
    CGRect indicatorFrame = CGRectMake(0, 0, kIndicatorSize, kIndicatorSize);
    _activityIndicatorView =
        [[UIActivityIndicatorView alloc] initWithFrame:indicatorFrame];
    _activityIndicatorView.activityIndicatorViewStyle =
        UIActivityIndicatorViewStyleMedium;
    _activityIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_activityIndicatorView];

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

    // Center indicator over `_leadingImageView`.
    AddSameCenterXConstraint(self, _leadingImageView, _activityIndicatorView);
    AddSameCenterYConstraint(self, _leadingImageView, _activityIndicatorView);

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
                 withTintColor:(UIColor*)trailingImageColor
       accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  _trailingButtonView.hidden = NO;
  _trailingButtonView.tintColor = trailingImageColor;
  [_trailingButtonView setImage:trailingButtonImage
                       forState:UIControlStateNormal];
  _trailingButtonView.accessibilityIdentifier = accessibilityIdentifier;
}

- (void)setLeadingIconImage:(UIImage*)image withTintColor:(UIColor*)tintColor {
  _leadingImageView.image = image;
  _leadingImageView.tintColor = tintColor;
  [_leadingImageView setHidden:image ? NO : YES];
}

- (void)showActivityIndicator {
  [_activityIndicatorView startAnimating];
  [_activityIndicatorView setHidden:NO];
  [_leadingImageView setHidden:YES];
}

- (void)hideActivityIndicator {
  [_activityIndicatorView stopAnimating];
  [_activityIndicatorView setHidden:YES];
  [_leadingImageView setHidden:NO];
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  self.textLabel.text = nil;
  self.accessibilityCustomActions = nil;
  [self setTrailingButtonImage:nil
                 withTintColor:nil
       accessibilityIdentifier:nil];
  [self setLeadingIconImage:nil withTintColor:nil];
  [_leadingImageView setHidden:YES];
  _trailingButtonView.hidden = YES;
  [self hideActivityIndicator];
}

#pragma mark - Private

- (void)trailingButtonTapped:(UIButton*)sender {
  [self.delegate didTapTrailingButton];
}

@end
