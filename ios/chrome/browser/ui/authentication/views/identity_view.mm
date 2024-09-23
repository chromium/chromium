// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/views/identity_view.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// List of constants related to the IdentityViewStyle.
typedef struct {
  const CGFloat avatarLeadingMargin;
  const CGFloat avatarSize;
  const CGFloat titleOffset;
  const CGFloat minimumTopMargin;
  const CGFloat minimumBottomMargin;
} StyleValues;

const StyleValues kDefaultStyle = {
    16., /* avatarLeadingMargin */
    40., /* avatarSize */
    4.,  /* titleOffset */
    12., /* minimumTopMargin */
    12., /* minimumBottomMargin */
};

const StyleValues kSignInChooseryStyle = {
    24., /* avatarLeadingMargin */
    40., /* avatarSize */
    4.,  /* titleOffset */
    7.,  /* minimumTopMargin */
    7.,  /* minimumBottomMargin */
};

const StyleValues kConsistencyStyle = {
    16., /* avatarLeadingMargin */
    30., /* avatarSize */
    0.,  /* titleOffset */
    10., /* minimumTopMargin */
    8.,  /* minimumBottomMargin */
};

// Distances/margins.
constexpr CGFloat kHorizontalAvatarLeadingMargin = 16.;

}  // namespace

@interface IdentityView ()

// Avatar.
@property(nonatomic, strong) UIImageView* avatarView;
// Contains the name if it exists, otherwise it contains the email.
@property(nonatomic, strong) UILabel* title;
// Contains the email if the name exists, otherwise it is hidden.
@property(nonatomic, strong) UILabel* subtitle;
// Constraints if the name exists.
@property(nonatomic, strong) NSLayoutConstraint* titleConstraintForNameAndEmail;
// Constraints if the name doesn't exist.
@property(nonatomic, strong) NSLayoutConstraint* titleConstraintForEmailOnly;
// Constraints to update when `self.minimumTopMargin` is updated.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* topConstraints;
// Constraints to update when `self.minimumBottomMargin` is updated.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* bottomConstraints;
// Constraints for the avatar size.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* avatarSizeConstraints;
// Leading margin constraint for the avatar.
@property(nonatomic, strong) NSLayoutConstraint* avatarLeadingMarginConstraint;

@end

@implementation IdentityView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.userInteractionEnabled = NO;
    // Avatar view.
    _avatarView = [[UIImageView alloc] init];
    _avatarView.translatesAutoresizingMaskIntoConstraints = NO;
    _avatarView.clipsToBounds = YES;
    [self addSubview:_avatarView];

    // Title.
    _title = [[UILabel alloc] init];
    _title.adjustsFontForContentSizeCategory = YES;
    _title.translatesAutoresizingMaskIntoConstraints = NO;
    _title.numberOfLines = 1;
    _title.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    _title.adjustsFontSizeToFitWidth = NO;
    _title.lineBreakMode = NSLineBreakByTruncatingTail;

    // Subtitle.
    _subtitle = [[UILabel alloc] init];
    _subtitle.adjustsFontForContentSizeCategory = YES;
    _subtitle.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitle.numberOfLines = 1;
    _subtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    _subtitle.adjustsFontSizeToFitWidth = NO;
    _subtitle.lineBreakMode = NSLineBreakByTruncatingTail;

    // Text container.
    UIStackView* contentView =
        [[UIStackView alloc] initWithArrangedSubviews:@[ _title, _subtitle ]];
    contentView.axis = UILayoutConstraintAxisVertical;
    contentView.distribution = UIStackViewDistributionEqualSpacing;
    contentView.alignment = UIStackViewAlignmentLeading;
    contentView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:contentView];

    // Layout constraints.
    _avatarLeadingMarginConstraint = [_avatarView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kDefaultStyle.avatarLeadingMargin];
    [NSLayoutConstraint activateConstraints:@[
      [contentView.leadingAnchor
          constraintEqualToAnchor:_avatarView.trailingAnchor
                         constant:kHorizontalAvatarLeadingMargin],
      _avatarLeadingMarginConstraint,
    ]];
    _avatarSizeConstraints = @[
      [_avatarView.heightAnchor
          constraintEqualToConstant:kDefaultStyle.avatarSize],
      [_avatarView.widthAnchor
          constraintEqualToConstant:kDefaultStyle.avatarSize],
    ];
    [NSLayoutConstraint activateConstraints:_avatarSizeConstraints];
    AddSameCenterYConstraint(self, _avatarView);
    AddSameConstraintsToSides(_title, _subtitle,
                              LayoutSides::kLeading | LayoutSides::kTrailing);
    _titleConstraintForNameAndEmail =
        [_subtitle.topAnchor constraintEqualToAnchor:_title.bottomAnchor
                                            constant:kDefaultStyle.titleOffset];
    _titleConstraintForEmailOnly =
        [contentView.bottomAnchor constraintEqualToAnchor:_title.bottomAnchor];

    [NSLayoutConstraint activateConstraints:@[
      [self.centerYAnchor constraintEqualToAnchor:contentView.centerYAnchor],
      [contentView.leadingAnchor constraintEqualToAnchor:_title.leadingAnchor],
      [contentView.leadingAnchor
          constraintEqualToAnchor:_subtitle.leadingAnchor],
      [contentView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [contentView.topAnchor constraintEqualToAnchor:_title.topAnchor],
      [contentView.bottomAnchor constraintEqualToAnchor:_subtitle.bottomAnchor],
    ]];

    _topConstraints = @[
      [_avatarView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                      constant:kDefaultStyle.minimumTopMargin],
      [_title.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                      constant:kDefaultStyle.minimumTopMargin],
    ];
    [NSLayoutConstraint activateConstraints:_topConstraints];
    _bottomConstraints = @[
      [self.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:_avatarView.bottomAnchor
                                      constant:kDefaultStyle
                                                   .minimumBottomMargin],
      [self.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:_subtitle.bottomAnchor
                                      constant:kDefaultStyle
                                                   .minimumBottomMargin],
    ];
    [NSLayoutConstraint activateConstraints:_bottomConstraints];
    // Initialize the style.
    [self updateStyle];
  }
  return self;
}

#pragma mark - Setter

- (void)setAvatar:(UIImage*)avatarImage {
  if (!avatarImage) {
    self.avatarView.image = nil;
  } else {
    const StyleValues* style = [self styleValues];
    DCHECK_EQ(avatarImage.size.width, style->avatarSize);
    DCHECK_EQ(avatarImage.size.height, style->avatarSize);
    self.avatarView.image = avatarImage;
    self.avatarView.layer.cornerRadius = style->avatarSize / 2.0;
  }
}

- (void)setTitle:(NSString*)title subtitle:(NSString*)subtitle {
  DCHECK(title);
  self.title.text = title;
  if (!subtitle.length) {
    self.titleConstraintForNameAndEmail.active = NO;
    self.titleConstraintForEmailOnly.active = YES;
    self.subtitle.hidden = YES;
  } else {
    self.titleConstraintForEmailOnly.active = NO;
    self.titleConstraintForNameAndEmail.active = YES;
    self.subtitle.hidden = NO;
    self.subtitle.text = subtitle;
  }
}

- (void)setTitleColor:(UIColor*)color {
  self.title.textColor = color;
}

#pragma mark - properties

- (void)setTitleFont:(UIFont*)titleFont {
  self.title.font = titleFont;
}

- (UIFont*)titleFont {
  return self.title.font;
}

- (void)setSubtitleFont:(UIFont*)subtitleFont {
  self.subtitle.font = subtitleFont;
}

- (UIFont*)subtitleFont {
  return self.subtitleFont;
}

- (void)setStyle:(IdentityViewStyle)style {
  if (_style == style)
    return;
  _style = style;
  [self updateStyle];
}

#pragma mark - private

// Returns the default style values according to `self.style`.
- (const StyleValues*)styleValues {
  switch (self.style) {
    case IdentityViewStyleDefault:
      return &kDefaultStyle;
    case IdentityViewStyleIdentityChooser:
      return &kSignInChooseryStyle;
    case IdentityViewStyleConsistency:
      return &kConsistencyStyle;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

// Updates the current view according the style.
- (void)updateStyle {
  const StyleValues* style = [self styleValues];
  switch (self.style) {
    case IdentityViewStyleDefault:
      self.titleFont =
          [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
      self.subtitleFont =
          [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
      break;
    case IdentityViewStyleIdentityChooser:
      self.titleFont =
          [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
      self.subtitleFont =
          [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
      break;
    case IdentityViewStyleConsistency:
      self.titleFont = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      self.subtitleFont =
          [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
      break;
  }
  DCHECK(style);
  self.avatarLeadingMarginConstraint.constant = style->avatarLeadingMargin;
  for (NSLayoutConstraint* constraint in self.avatarSizeConstraints) {
    constraint.constant = style->avatarSize;
  }
  self.avatarView.layer.cornerRadius = style->avatarSize / 2.;
  for (NSLayoutConstraint* constraint in self.topConstraints) {
    constraint.constant = style->minimumTopMargin;
  }
  for (NSLayoutConstraint* constraint in self.bottomConstraints) {
    constraint.constant = style->minimumBottomMargin;
  }
  self.titleConstraintForNameAndEmail.constant = style->titleOffset;
}

@end
