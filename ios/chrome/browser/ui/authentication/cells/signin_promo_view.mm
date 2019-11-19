// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "build/branding_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_delegate.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Horizontal padding for label and buttons.
const CGFloat kHorizontalPadding = 40;
// Image size for warm state.
const CGFloat kProfileImageFixedSize = 48;

// UI Refresh Constants:
// Vertical spacing between stackView and cell contentView.
const CGFloat kStackViewVerticalPadding = 11.0;
// Horizontal spacing between stackView and cell contentView.
const CGFloat kStackViewHorizontalPadding = 16.0;
// Spacing within stackView.
const CGFloat kStackViewSubViewSpacing = 13.0;
// Horizontal Inset between button contents and edge.
const CGFloat kButtonTitleHorizontalContentInset = 40.0;
// Vertical Inset between button contents and edge.
const CGFloat kButtonTitleVerticalContentInset = 8.0;
// Button corner radius.
const CGFloat kButtonCornerRadius = 8;
// Trailing margin for the close button.
const CGFloat kCloseButtonTrailingMargin = 5;
// Size for the close button width and height.
const CGFloat kCloseButtonWidthHeight = 24;
// Size for the imageView width and height.
const CGFloat kImageViewWidthHeight = 32;
}

@interface SigninPromoView ()
// Re-declare as readwrite.
@property(nonatomic, readwrite) UIImageView* imageView;
@property(nonatomic, readwrite) UILabel* textLabel;
@property(nonatomic, readwrite) UIButton* primaryButton;
@property(nonatomic, readwrite) UIButton* secondaryButton;
@property(nonatomic, readwrite) UIButton* closeButton;
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

    // Create and setup informative text label.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.numberOfLines = 0;
    _textLabel.textAlignment = NSTextAlignmentCenter;
    _textLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _textLabel.textColor = UIColor.cr_labelColor;

    // Create and setup primary button.
    UIButton* primaryButton;
    UIEdgeInsets primaryButtonInsets;
    primaryButton = [[UIButton alloc] init];
    primaryButton.backgroundColor = [UIColor colorNamed:kBlueColor];
    [primaryButton.titleLabel
        setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]];
    primaryButton.layer.cornerRadius = kButtonCornerRadius;
    primaryButton.clipsToBounds = YES;
    primaryButtonInsets = UIEdgeInsetsMake(
        kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset,
        kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset);
    _primaryButton = primaryButton;
    DCHECK(_primaryButton);
    _primaryButton.accessibilityIdentifier = kSigninPromoPrimaryButtonId;
    [_primaryButton setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                         forState:UIControlStateNormal];
    _primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
    _primaryButton.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    [_primaryButton addTarget:self
                       action:@selector(onPrimaryButtonAction:)
             forControlEvents:UIControlEventTouchUpInside];
    _primaryButton.contentEdgeInsets = primaryButtonInsets;

    // Create and setup seconday button.
    UIButton* secondaryButton;
    secondaryButton = [[UIButton alloc] init];
    [secondaryButton.titleLabel
        setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]];
    [secondaryButton setTitleColor:[UIColor colorNamed:kBlueColor]
                          forState:UIControlStateNormal];
    _secondaryButton = secondaryButton;
    DCHECK(_secondaryButton);
    _secondaryButton.translatesAutoresizingMaskIntoConstraints = NO;
    _secondaryButton.accessibilityIdentifier = kSigninPromoSecondaryButtonId;
    [_secondaryButton addTarget:self
                         action:@selector(onSecondaryButtonAction:)
               forControlEvents:UIControlEventTouchUpInside];

    // Vertical stackView containing all previous view.
    UIStackView* verticalStackView =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          _imageView, _textLabel, _primaryButton, _secondaryButton
        ]];
    verticalStackView.alignment = UIStackViewAlignmentCenter;
    verticalStackView.axis = UILayoutConstraintAxisVertical;
    verticalStackView.translatesAutoresizingMaskIntoConstraints = NO;
    verticalStackView.spacing = kStackViewSubViewSpacing;
    [self addSubview:verticalStackView];

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
    [self addSubview:_closeButton];

    [NSLayoutConstraint activateConstraints:@[
      [verticalStackView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kStackViewHorizontalPadding],
      [verticalStackView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kStackViewHorizontalPadding],
      [verticalStackView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kStackViewVerticalPadding],
      [verticalStackView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-kStackViewVerticalPadding],
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
    // Default mode.
    _mode = SigninPromoViewModeColdState;
    [self activateColdMode];
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
    case SigninPromoViewModeColdState:
      [self activateColdMode];
      return;
    case SigninPromoViewModeWarmState:
      [self activateWarmMode];
      return;
  }
  NOTREACHED();
}

- (void)activateColdMode {
  DCHECK_EQ(_mode, SigninPromoViewModeColdState);
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

- (void)activateWarmMode {
  DCHECK_EQ(_mode, SigninPromoViewModeWarmState);
  _secondaryButton.hidden = NO;
}

- (void)setProfileImage:(UIImage*)image {
  DCHECK_EQ(SigninPromoViewModeWarmState, _mode);
  self.imageView.image = CircularImageFromImage(image, kProfileImageFixedSize);
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
    case SigninPromoViewModeColdState:
      [_delegate signinPromoViewDidTapSigninWithNewAccount:self];
      break;
    case SigninPromoViewModeWarmState:
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

- (BOOL)accessibilityActivate {
  [self accessibilityPrimaryAction:nil];
  return YES;
}

- (NSArray<UIAccessibilityCustomAction*>*)accessibilityCustomActions {
  NSMutableArray* actions = [NSMutableArray array];

  if (_mode == SigninPromoViewModeWarmState) {
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
