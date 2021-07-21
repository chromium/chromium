// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/new_password_table_cell.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Margin on either side of the content.
const CGFloat kHorizontalMargin = 20;

// Vertical margin on both sides of the content in small content size
// categories.
const CGFloat kVerticalMarginSmall = 12;

// Vertical margin on both sides of the content in large content size
// categories.
const CGFloat kVerticalMarginLarge = 20;

// Spacing between the two text views. Taken from the default UITableViewCell.
const CGFloat kTextViewSpacing = 6;

// Spacing between the button and the text. Taken from the default
// UITableViewCell.
const CGFloat kButtonSpacing = 8;

}  // namespace

@interface NewPasswordTableCell ()

// Label for the row title.
@property(nonatomic, strong) UILabel* titleLabel;

// Button to toggle hiding or showing the password.
@property(nonatomic, strong) UIButton* hidePasswordButton;

// Vertical margin for the cell content. This allows the margin to be increased
// when the |preferredContentSizeCategory| changes.
@property(nonatomic, strong)
    NSLayoutConstraint* contentVerticalMarginConstraint;

// Stack view holding the two text items. This is normally laid out horizontally
// but switches to vertical in large content size categories.
@property(nonatomic, strong) UIStackView* textStackView;

// Overall stack view holding the text elements and the password button.
@property(nonatomic, strong) UIStackView* overallStackView;

// Whether or not the password is currently hidden.
@property(nonatomic, assign) BOOL passwordHidden;

// The type of this cell.
@property(nonatomic, assign) NewPasswordTableCellType cellType;

// Convenience accessor for the hide password image.
@property(nonatomic, readonly, strong) UIImage* hidePasswordImage;

// Convenience accessor for the reveal password image.
@property(nonatomic, readonly, strong) UIImage* revealPasswordImage;

@end

@implementation NewPasswordTableCell

+ (NSString*)reuseID {
  return @"NewPasswordTableCell";
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  if (self = [super initWithStyle:style reuseIdentifier:reuseIdentifier]) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.numberOfLines = 0;

    _textField = [[UITextField alloc] init];
    _textField.translatesAutoresizingMaskIntoConstraints = NO;
    _textField.hidden = YES;
    _textField.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textField.textColor = [UIColor colorNamed:kTextPrimaryColor];

    _hidePasswordButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _hidePasswordButton.translatesAutoresizingMaskIntoConstraints = NO;
    _hidePasswordButton.hidden = YES;
    [_hidePasswordButton addTarget:self
                            action:@selector(hidePasswordTapped:)
                  forControlEvents:UIControlEventTouchUpInside];
    [_hidePasswordButton setImage:self.hidePasswordImage
                         forState:UIControlStateNormal];
    _hidePasswordButton.tintColor = [UIColor colorNamed:kBlueColor];

    // The button should neither shrink nor grow.
    [_hidePasswordButton
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisHorizontal];
    [_hidePasswordButton
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];

    _textStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _textField ]];
    _textStackView.translatesAutoresizingMaskIntoConstraints = NO;

    _overallStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textStackView, _hidePasswordButton ]];
    _overallStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _overallStackView.spacing = kButtonSpacing;

    [self.contentView addSubview:_overallStackView];

    self.contentVerticalMarginConstraint = [_overallStackView.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor];

    [NSLayoutConstraint activateConstraints:@[
      [_overallStackView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kHorizontalMargin],
      [self.contentView.trailingAnchor
          constraintEqualToAnchor:_overallStackView.trailingAnchor
                         constant:kHorizontalMargin],
      self.contentVerticalMarginConstraint,
      [_overallStackView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];

    [self updateViewsForContentSizeCategory:self.traitCollection
                                                .preferredContentSizeCategory];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.titleLabel.text = @"";
  self.titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  self.textField.hidden = YES;
  self.textField.placeholder = @"";
  self.hidePasswordButton.hidden = YES;
  self.passwordHidden = NO;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [self updateViewsForContentSizeCategory:self.traitCollection
                                              .preferredContentSizeCategory];
}

// Updates the values that change based on the given |contentSizeCategory|.
- (void)updateViewsForContentSizeCategory:
    (UIContentSizeCategory)contentSizeCategory {
  BOOL sizeCategoryIsLarge =
      contentSizeCategory >= UIContentSizeCategoryAccessibilityMedium;

  self.textStackView.axis = (sizeCategoryIsLarge)
                                ? UILayoutConstraintAxisVertical
                                : UILayoutConstraintAxisHorizontal;
  self.textStackView.spacing = (sizeCategoryIsLarge) ? 0 : kTextViewSpacing;
  self.textField.textAlignment =
      (sizeCategoryIsLarge) ? NSTextAlignmentLeft : NSTextAlignmentRight;

  // The actual UITableViewCell uses a scaling vertical margin based on the
  // |contentSizeCategory|. This is close enough and doesn't require unique
  // margins for each different size.
  self.contentVerticalMarginConstraint.constant =
      (sizeCategoryIsLarge) ? kVerticalMarginLarge : kVerticalMarginSmall;
}

#pragma mark - Actions

- (void)hidePasswordTapped:(id)sender {
  self.passwordHidden = !self.passwordHidden;
}

#pragma mark - Accessors

- (UIImage*)hidePasswordImage {
  return [[UIImage imageNamed:@"password_hide_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

- (UIImage*)revealPasswordImage {
  return [[UIImage imageNamed:@"password_reveal_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

- (void)setPasswordHidden:(BOOL)passwordHidden {
  _passwordHidden = passwordHidden;
  self.textField.secureTextEntry = _passwordHidden;

  UIImage* newImage =
      _passwordHidden ? self.revealPasswordImage : self.hidePasswordImage;
  [self.hidePasswordButton setImage:newImage forState:UIControlStateNormal];
}

// After |-prepareForReuse|, all views in the stack view are hidden. This method
// prepares the stack view contents to display the correct cell type.
- (void)setCellType:(NewPasswordTableCellType)cellType {
  _cellType = cellType;

  switch (cellType) {
    case NewPasswordTableCellTypeUsername:
      self.titleLabel.text = NSLocalizedString(
          @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_USERNAME", @"Username");

      self.textField.hidden = NO;
      self.textField.placeholder = NSLocalizedString(
          @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_USERNAME_PLACEHOLDER",
          @"Placeholder marking username field as optional");
      break;
    case NewPasswordTableCellTypePassword:
      self.titleLabel.text = NSLocalizedString(
          @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_PASSWORD", @"Password");

      self.textField.hidden = NO;
      self.textField.placeholder = NSLocalizedString(
          @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_PASSWORD_PLACEHOLDER",
          @"Placeholder for password field");

      self.hidePasswordButton.hidden = NO;
      self.passwordHidden = YES;
      break;
    case NewPasswordTableCellTypeSuggestStrongPassword:
      self.titleLabel.text = NSLocalizedString(
          @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_SUGGEST_STRONG_PASSWORD",
          @"Button allowing users to request Chrome suggest a strong password");
      self.titleLabel.textColor = [UIColor colorNamed:kBlueColor];
      break;
    case NewPasswordTableCellTypeNumRows:
      break;
  }
}

@end
