// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"

#include "base/check_op.h"
#include "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "build/branding_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_delegate.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Horizontal padding for label and buttons.
constexpr CGFloat kHorizontalPadding = 40;

// UI Refresh Constants:
// Vertical spacing between stackView and cell contentView.
constexpr CGFloat kStackViewVerticalPadding = 11.0;
// Horizontal spacing between stackView and cell contentView.
constexpr CGFloat kStackViewHorizontalPadding = 16.0;
// Spacing within stackView.
constexpr CGFloat kStackViewSubViewSpacing = 13.0;
// Horizontal Inset between button contents and edge.
constexpr CGFloat kButtonTitleHorizontalContentInset = 12.0;
// Vertical Inset between button contents and edge.
constexpr CGFloat kButtonTitleVerticalContentInset = 8.0;
// Button corner radius.
constexpr CGFloat kButtonCornerRadius = 8;
// Trailing margin for the close button.
constexpr CGFloat kCloseButtonTrailingMargin = 5;
// Size for the close button width and height.
constexpr CGFloat kCloseButtonWidthHeight = 24;
// Size for the imageView width and height.
constexpr CGFloat kImageViewWidthHeight = 32;
}

@interface SigninPromoView ()
// Re-declare as readwrite.
@property(nonatomic, strong, readwrite) UIImageView* imageView;
@property(nonatomic, strong, readwrite) UILabel* titleLabel;
@property(nonatomic, strong, readwrite) UILabel* textLabel;
@property(nonatomic, strong, readwrite) UIButton* primaryButton;
@property(nonatomic, strong, readwrite) UIButton* secondaryButton;
@property(nonatomic, strong, readwrite) UIButton* closeButton;
// Contains the two main sections of the promo (image and Text).
@property(nonatomic, strong) UIStackView* contentStackView;
// Contains all the text elements of the promo (title,body and buttons).
@property(nonatomic, strong) UIStackView* textVerticalStackView;
@end

@implementation SigninPromoView {
  signin_metrics::AccessPoint _accessPoint;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Set the whole element as accessible to take advantage of the
    // accessibilityCustomActions.
    self.isAccessibilityElement = YES;
    self.accessibilityIdentifier = kSigninPromoViewId;

    // Create and setup imageview that will hold the browser icon or user
    // profile image.
    _imageView = [[UIImageView alloc] init];
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    _imageView.layer.masksToBounds = YES;
    _imageView.contentMode = UIViewContentModeScaleAspectFit;

    // Create and setup title label.
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.numberOfLines = 0;
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle3];
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    // Title is hidden by default.
    _titleLabel.hidden = YES;

    // Create and setup informative text label.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.numberOfLines = 0;
    _textLabel.textAlignment = NSTextAlignmentCenter;
    _textLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

    // Create and setup primary button.
    UIEdgeInsets primaryButtonInsets;
    _primaryButton = [[UIButton alloc] init];
    _primaryButton.backgroundColor = [UIColor colorNamed:kBlueColor];
    [_primaryButton.titleLabel
        setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]];
    _primaryButton.titleLabel.adjustsFontSizeToFitWidth = YES;
    _primaryButton.titleLabel.minimumScaleFactor = 0.7;

    _primaryButton.layer.cornerRadius = kButtonCornerRadius;
    _primaryButton.clipsToBounds = YES;
    primaryButtonInsets = UIEdgeInsetsMake(
        kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset,
        kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset);
    _primaryButton.accessibilityIdentifier = kSigninPromoPrimaryButtonId;
    [_primaryButton setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                         forState:UIControlStateNormal];
    _primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
    _primaryButton.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    [_primaryButton addTarget:self
                       action:@selector(onPrimaryButtonAction:)
             forControlEvents:UIControlEventTouchUpInside];
    _primaryButton.contentEdgeInsets = primaryButtonInsets;
    _primaryButton.pointerInteractionEnabled = YES;
    _primaryButton.pointerStyleProvider =
        CreateOpaqueButtonPointerStyleProvider();

    // Create and setup seconday button.
    _secondaryButton = [[UIButton alloc] init];
    [_secondaryButton.titleLabel
        setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]];
    [_secondaryButton setTitleColor:[UIColor colorNamed:kBlueColor]
                           forState:UIControlStateNormal];
    _secondaryButton.translatesAutoresizingMaskIntoConstraints = NO;
    _secondaryButton.accessibilityIdentifier = kSigninPromoSecondaryButtonId;
    [_secondaryButton addTarget:self
                         action:@selector(onSecondaryButtonAction:)
               forControlEvents:UIControlEventTouchUpInside];
    _secondaryButton.pointerInteractionEnabled = YES;

    _textVerticalStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _titleLabel, _textLabel, _primaryButton, _secondaryButton
    ]];

    _textVerticalStackView.alignment = UIStackViewAlignmentCenter;
    _textVerticalStackView.axis = UILayoutConstraintAxisVertical;
    _textVerticalStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _textVerticalStackView.spacing = kStackViewSubViewSpacing;

    _contentStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _imageView, _textVerticalStackView ]];
    _contentStackView.alignment = UIStackViewAlignmentCenter;
    _contentStackView.axis = UILayoutConstraintAxisVertical;
    _contentStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _contentStackView.spacing = kStackViewSubViewSpacing;

    [self addSubview:_contentStackView];

    // Create close button and adds it directly to self.
    _closeButton = [[UIButton alloc] init];
    _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    _closeButton.accessibilityIdentifier = kSigninPromoCloseButtonId;
    [_closeButton addTarget:self
                     action:@selector(onCloseButtonAction:)
           forControlEvents:UIControlEventTouchUpInside];
    [_closeButton setImage:[UIImage imageNamed:@"signin_promo_close_gray"]
                  forState:UIControlStateNormal];
    _closeButton.hidden = YES;
    _closeButton.pointerInteractionEnabled = YES;
    [self addSubview:_closeButton];

    [NSLayoutConstraint activateConstraints:@[
      [_contentStackView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kStackViewHorizontalPadding],
      [_contentStackView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kStackViewHorizontalPadding],
      [_contentStackView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kStackViewVerticalPadding],
      [_contentStackView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-kStackViewVerticalPadding],
      // Image view.
      [_imageView.heightAnchor constraintEqualToConstant:kImageViewWidthHeight],
      [_imageView.widthAnchor constraintEqualToConstant:kImageViewWidthHeight],
      // Close button constraints.
      [_closeButton.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_closeButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:kCloseButtonTrailingMargin],
      [_closeButton.heightAnchor
          constraintEqualToConstant:kCloseButtonWidthHeight],
      [_closeButton.widthAnchor
          constraintEqualToConstant:kCloseButtonWidthHeight],
    ]];
    // Default layout.
    _compactLayout = NO;
    // Default mode.
    _mode = SigninPromoViewModeNoAccounts;
    [self activateNoAccountsMode];
  }
  return self;
}

- (void)prepareForReuse {
  _delegate = nil;
}

- (void)setMode:(SigninPromoViewMode)mode {
  if (mode == _mode) {
    return;
  }
  _mode = mode;
  switch (_mode) {
    case SigninPromoViewModeNoAccounts:
      [self activateNoAccountsMode];
      return;
    case SigninPromoViewModeSigninWithAccount:
      [self activateSigninWithAccountMode];
      return;
    case SigninPromoViewModeSyncWithPrimaryAccount:
      [self activateSyncWithPrimaryAccountMode];
      return;
  }
  NOTREACHED();
}

- (void)activateNoAccountsMode {
  DCHECK_EQ(_mode, SigninPromoViewModeNoAccounts);
  UIImage* logo = nil;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  logo = [UIImage imageNamed:@"signin_promo_logo_chrome_color"];
#else
  logo = [UIImage imageNamed:@"signin_promo_logo_chromium_color"];
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  DCHECK(logo);
  _imageView.image = logo;
  _secondaryButton.hidden = YES;
}

- (void)activateSigninWithAccountMode {
  DCHECK_EQ(_mode, SigninPromoViewModeSigninWithAccount);
  _secondaryButton.hidden = NO;
}

- (void)activateSyncWithPrimaryAccountMode {
  DCHECK_EQ(_mode, SigninPromoViewModeSyncWithPrimaryAccount);
  _secondaryButton.hidden = YES;
}

- (void)setProfileImage:(UIImage*)image {
  DCHECK_NE(_mode, SigninPromoViewModeNoAccounts);
  DCHECK_EQ(kImageViewWidthHeight, image.size.width);
  DCHECK_EQ(kImageViewWidthHeight, image.size.height);
  self.imageView.image = CircularImageFromImage(image, kImageViewWidthHeight);
}

- (void)setNonProfileImage:(UIImage*)image {
  DCHECK_EQ(_mode, SigninPromoViewModeNoAccounts);
  DCHECK_EQ(kImageViewWidthHeight, image.size.width);
  DCHECK_EQ(kImageViewWidthHeight, image.size.height);
  self.imageView.image = image;
}

- (void)setCompactLayout:(BOOL)compactLayout {
  if (compactLayout == _compactLayout)
    return;
  _compactLayout = compactLayout;
  if (_compactLayout) {
    _contentStackView.axis = UILayoutConstraintAxisVertical;
    _textVerticalStackView.alignment = UIStackViewAlignmentLeading;
    _textLabel.textAlignment = NSTextAlignmentNatural;
    // In compact layout the primary button is plain.
    _primaryButton.backgroundColor = nil;
    [_primaryButton setTitleColor:[UIColor colorNamed:kBlueColor]
                         forState:UIControlStateNormal];
    _primaryButton.layer.cornerRadius = 0.0;
    _primaryButton.clipsToBounds = NO;
  } else {
    _contentStackView.axis = UILayoutConstraintAxisHorizontal;
    _textVerticalStackView.alignment = UIStackViewAlignmentCenter;
    _textLabel.textAlignment = NSTextAlignmentCenter;
    [_primaryButton setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                         forState:UIControlStateNormal];
    _primaryButton.backgroundColor = [UIColor colorNamed:kBlueColor];
    _primaryButton.layer.cornerRadius = kButtonCornerRadius;
    _primaryButton.clipsToBounds = YES;
  }
}

- (void)accessibilityPrimaryAction:(id)unused {
  [self.primaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];
}

- (void)accessibilitySecondaryAction:(id)unused {
  [self.secondaryButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
}

- (void)accessibilityCloseAction:(id)unused {
  [self.closeButton sendActionsForControlEvents:UIControlEventTouchUpInside];
}

- (CGFloat)horizontalPadding {
  return kHorizontalPadding;
}

- (void)onPrimaryButtonAction:(id)unused {
  switch (_mode) {
    case SigninPromoViewModeNoAccounts:
      [_delegate signinPromoViewDidTapSigninWithNewAccount:self];
      break;
    case SigninPromoViewModeSigninWithAccount:
    case SigninPromoViewModeSyncWithPrimaryAccount:
      [_delegate signinPromoViewDidTapSigninWithDefaultAccount:self];
      break;
  }
}

- (void)onSecondaryButtonAction:(id)unused {
  [_delegate signinPromoViewDidTapSigninWithOtherAccount:self];
}

- (void)onCloseButtonAction:(id)unused {
  [_delegate signinPromoViewCloseButtonWasTapped:self];
}

#pragma mark - NSObject(Accessibility)

- (void)setAccessibilityLabel:(NSString*)accessibilityLabel {
  NOTREACHED();
}

- (NSString*)accessibilityLabel {
  return [NSString
      stringWithFormat:@"%@ %@", self.textLabel.text,
                       [self.primaryButton titleForState:UIControlStateNormal]];
}

- (BOOL)accessibilityActivate {
  [self accessibilityPrimaryAction:nil];
  return YES;
}

- (NSArray<UIAccessibilityCustomAction*>*)accessibilityCustomActions {
  NSMutableArray* actions = [NSMutableArray array];

  if (_mode == SigninPromoViewModeSigninWithAccount) {
    NSString* secondaryActionName =
        [self.secondaryButton titleForState:UIControlStateNormal];
    UIAccessibilityCustomAction* secondaryCustomAction =
        [[UIAccessibilityCustomAction alloc]
            initWithName:secondaryActionName
                  target:self
                selector:@selector(accessibilitySecondaryAction:)];
    [actions addObject:secondaryCustomAction];
  }

  if (!self.closeButton.hidden) {
    NSString* closeActionName =
        l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_CLOSE_ACCESSIBILITY);
    UIAccessibilityCustomAction* closeCustomAction =
        [[UIAccessibilityCustomAction alloc]
            initWithName:closeActionName
                  target:self
                selector:@selector(accessibilityCloseAction:)];
    [actions addObject:closeCustomAction];
  }

  return actions;
}

@end
