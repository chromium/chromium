// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
const CGFloat kSymbolSize = 22;
}

@interface SettingsCheckCell ()

// The image view for the trailing image.
@property(nonatomic, strong) UIImageView* trailingImageView;

// UIActivityIndicatorView spinning while check is running.
@property(nonatomic, readonly, strong)
    UIActivityIndicatorView* activityIndicator;

// Image view for the leading image.
@property(nonatomic, strong) UIImageView* leadingImageView;

// Constraint that is used to define trailing text constraint without
// `trailingImageView` `activityIndicator` and `infoButton`.
@property(nonatomic, strong)
    NSLayoutConstraint* textNoTrailingContentsConstraint;

// Constraint that is used to define trailing text constraint with either
// `trailingImageView` or `activityIndicator` or `infoButton` showing.
@property(nonatomic, strong)
    NSLayoutConstraint* textWithTrailingContentsConstraint;

// Constraint used for leading text constraint without `leadingImage`.
@property(nonatomic, strong) NSLayoutConstraint* textNoLeadingImageConstraint;

// Constraint used for leading text constraint with `leadingImage` showing.
@property(nonatomic, strong) NSLayoutConstraint* textWithLeadingImageConstraint;

@end

@implementation SettingsCheckCell {
  UIView* _leadingIconBackground;
}

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    UIView* contentView = self.contentView;

    // Attributes of row contents in order or appearance (if present).

    // `_leadingImageView` attributes
    _leadingIconBackground = [[UIView alloc] init];
    _leadingIconBackground.translatesAutoresizingMaskIntoConstraints = NO;
    _leadingIconBackground.hidden = NO;
    [contentView addSubview:_leadingIconBackground];

    _leadingImageView = [[UIImageView alloc] init];
    _leadingImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _leadingImageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
    _leadingImageView.contentMode = UIViewContentModeCenter;
    [contentView addSubview:_leadingImageView];

    AddSameCenterConstraints(_leadingImageView, _leadingIconBackground);

    // Text attributes.
    // `_textLabel` attributes.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    [contentView addSubview:_textLabel];
    // `detailText` attributes.
    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.numberOfLines = 0;
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [contentView addSubview:_detailTextLabel];

    // Only `_trailingImageView` or `_activityIndicator` or `_infoButton` is
    // shown, not all at once. `trailingImage` attributes.
    _trailingImageView = [[UIImageView alloc] init];
    _trailingImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _trailingImageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
    _trailingImageView.hidden = YES;
    [contentView addSubview:_trailingImageView];
    // `activityIndictor` attributes.
    // Creates default activity indicator. Color depends on appearance.
    _activityIndicator = [[UIActivityIndicatorView alloc] init];
    _activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
    _activityIndicator.hidden = YES;
    [contentView addSubview:_activityIndicator];
    // `_infoButton` attribues.
    _infoButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _infoButton.translatesAutoresizingMaskIntoConstraints = NO;
    _infoButton.hidden = YES;
    UIImage* image = DefaultSymbolWithPointSize(kInfoCircleSymbol, kSymbolSize);
    [_infoButton setImage:image forState:UIControlStateNormal];
    [_infoButton setTintColor:[UIColor colorNamed:kBlueColor]];
    [contentView addSubview:_infoButton];

    // Constraints.
    UILayoutGuide* textLayoutGuide = [[UILayoutGuide alloc] init];
    [self.contentView addLayoutGuide:textLayoutGuide];

    _textNoTrailingContentsConstraint = [textLayoutGuide.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kTableViewHorizontalSpacing];

    _textWithTrailingContentsConstraint = [textLayoutGuide.trailingAnchor
        constraintEqualToAnchor:_trailingImageView.leadingAnchor
                       constant:-kTableViewImagePadding];

    _textNoLeadingImageConstraint = [textLayoutGuide.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing];

    _textWithLeadingImageConstraint = [textLayoutGuide.leadingAnchor
        constraintEqualToAnchor:_leadingIconBackground.trailingAnchor
                       constant:kTableViewImagePadding];

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];

    // Set up the constraints assuming that the icon image and activity
    // indicator are hidden.
    [NSLayoutConstraint activateConstraints:@[
      heightConstraint,
      _textNoTrailingContentsConstraint,

      // Constraints for `_trailingImageView` (same position as
      // `_activityIndictor`).
      [_trailingImageView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_trailingImageView.widthAnchor
          constraintEqualToConstant:kTableViewIconImageSize],
      [_trailingImageView.heightAnchor
          constraintEqualToAnchor:_trailingImageView.widthAnchor],
      [_trailingImageView.centerYAnchor
          constraintEqualToAnchor:textLayoutGuide.centerYAnchor],
      [_trailingImageView.leadingAnchor
          constraintEqualToAnchor:_activityIndicator.leadingAnchor],

      // Constraints for `_infoButton` (same position as
      // `_trailingImageView`).
      [_infoButton.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_infoButton.widthAnchor
          constraintEqualToConstant:kTableViewIconImageSize],
      [_infoButton.heightAnchor
          constraintEqualToAnchor:_infoButton.widthAnchor],
      [_infoButton.centerYAnchor
          constraintEqualToAnchor:textLayoutGuide.centerYAnchor],
      [_infoButton.leadingAnchor
          constraintEqualToAnchor:_activityIndicator.leadingAnchor],

      // Constraints for `_activityIndictor` (same position as
      // `_trailingImageView`).
      [_activityIndicator.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_activityIndicator.widthAnchor
          constraintEqualToConstant:kTableViewIconImageSize],
      [_activityIndicator.heightAnchor
          constraintEqualToAnchor:_activityIndicator.widthAnchor],
      [_activityIndicator.centerYAnchor
          constraintEqualToAnchor:textLayoutGuide.centerYAnchor],

      // Constraints for `_leadingIconBackground`.
      [_leadingIconBackground.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_leadingIconBackground.widthAnchor
          constraintEqualToConstant:kTableViewIconImageSize],
      [_leadingIconBackground.heightAnchor
          constraintEqualToAnchor:_leadingIconBackground.widthAnchor],
      [_leadingIconBackground.centerYAnchor
          constraintEqualToAnchor:textLayoutGuide.centerYAnchor],

      // Constraints for `_textLabel` and `_detailTextLabel`.
      [textLayoutGuide.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [textLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_textLabel.leadingAnchor],
      [textLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_detailTextLabel.leadingAnchor],
      [textLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_textLabel.trailingAnchor],
      [textLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_detailTextLabel.trailingAnchor],
      [textLayoutGuide.topAnchor constraintEqualToAnchor:_textLabel.topAnchor],
      [textLayoutGuide.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.bottomAnchor],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.topAnchor],
    ]];
    // Make sure there are top and bottom margins of at least `margin`.
    AddOptionalVerticalPadding(self.contentView, textLayoutGuide,
                               kTableViewTwoLabelsCellVerticalSpacing);
  }
  return self;
}

- (void)showActivityIndicator {
  if (!self.activityIndicator.hidden)
    return;
  self.trailingImageView.hidden = YES;
  self.infoButton.hidden = YES;
  self.activityIndicator.hidden = NO;
  [self.activityIndicator startAnimating];
  [self updateTrailingImageTextConstraints];
}

- (void)hideActivityIndicator {
  if (self.activityIndicator.hidden)
    return;

  [self.activityIndicator stopAnimating];
  self.activityIndicator.hidden = YES;
  [self updateTrailingImageTextConstraints];
}

- (void)setTrailingImage:(UIImage*)trailingImage
           withTintColor:(UIColor*)trailingImageColor {
  self.trailingImageView.tintColor = trailingImageColor;
  BOOL hidden = !trailingImage;
  self.trailingImageView.image = trailingImage;
  if (hidden == self.trailingImageView.hidden)
    return;
  self.trailingImageView.hidden = hidden;
  if (!hidden) {
    self.activityIndicator.hidden = YES;
    self.infoButton.hidden = YES;
  }
  [self updateTrailingImageTextConstraints];
}

- (void)setLeadingIconImage:(UIImage*)image
                  tintColor:(UIColor*)tintColor
            backgroundColor:(UIColor*)backgroundColor
               cornerRadius:(CGFloat)cornerRadius {
  self.leadingImageView.image = image;
  self.leadingImageView.tintColor = tintColor;

  _leadingIconBackground.backgroundColor = backgroundColor;
  _leadingIconBackground.layer.cornerRadius = cornerRadius;

  BOOL hidden = !image;
  _leadingIconBackground.hidden = hidden;
  // Update the leading text constraint based on `image` being provided.
  if (hidden) {
    _textWithLeadingImageConstraint.active = NO;
    _textNoLeadingImageConstraint.active = YES;
  } else {
    _textNoLeadingImageConstraint.active = NO;
    _textWithLeadingImageConstraint.active = YES;
  }
}

- (void)setInfoButtonHidden:(BOOL)hidden {
  if (hidden == self.infoButton.hidden)
    return;

  self.infoButton.hidden = hidden;
  if (hidden) {
    self.accessibilityCustomActions = nil;
  } else {
    self.accessibilityCustomActions = [self createAccessibilityActions];
    self.trailingImageView.hidden = YES;
    self.activityIndicator.hidden = YES;
  }
  [self updateTrailingImageTextConstraints];
}

- (void)setInfoButtonEnabled:(BOOL)enabled {
  self.infoButton.enabled = enabled;
  if (enabled) {
    [self.infoButton setTintColor:[UIColor colorNamed:kBlueColor]];
  } else {
    [self.infoButton setTintColor:[UIColor colorNamed:kTextSecondaryColor]];
  }
}

#pragma mark - Private Methods

// Updates the constraints around the trailing image for when `trailingImage` or
// `activityIndicator` or `infoButton` is shown or hidden.
- (void)updateTrailingImageTextConstraints {
  // Active proper `textLayoutGuide` trailing constraint to show
  // `trailingImageView` or `activityIndicator` or `infoButton`.
  if (self.activityIndicator.hidden && self.trailingImageView.hidden &&
      self.infoButton.hidden) {
    _textWithTrailingContentsConstraint.active = NO;
    _textNoTrailingContentsConstraint.active = YES;
  } else {
    _textNoTrailingContentsConstraint.active = NO;
    _textWithTrailingContentsConstraint.active = YES;
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  self.textLabel.text = nil;
  self.accessibilityCustomActions = nil;
  [self setInfoButtonEnabled:YES];
  [self.infoButton removeTarget:nil
                         action:nil
               forControlEvents:UIControlEventAllEvents];
  self.detailTextLabel.text = nil;
  self.accessibilityTraits = UIAccessibilityTraitNone;
  [self setTrailingImage:nil withTintColor:nil];
  [self setLeadingIconImage:nil
                  tintColor:nil
            backgroundColor:nil
               cornerRadius:0];
  [self hideActivityIndicator];
}

#pragma mark - Accessibility

// Creates custom accessibility actions.
- (NSArray*)createAccessibilityActions {
  UIAccessibilityCustomAction* tapButtonAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_INFO_BUTTON_ACCESSIBILITY_HINT)
                target:self
              selector:@selector(handleInfoButtonTapForCell)];
  return @[ tapButtonAction ];
}

// Handles accessibility action for tapping outside the info button.
- (void)handleInfoButtonTapForCell {
  [self.infoButton sendActionsForControlEvents:UIControlEventTouchUpInside];
}

@end
