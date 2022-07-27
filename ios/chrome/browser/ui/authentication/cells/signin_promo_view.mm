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

typedef struct {
  // Vertical spacing between stackView and cell contentView.
  const CGFloat kStackViewVerticalPadding;
  // Trailing margin for content.
  const CGFloat kStackViewTrailingMargin;
  // Spacing within content stack view.
  const CGFloat kContentStackViewSubViewSpacing;
  // Spacing within text stack view.
  const CGFloat kTextStackViewSubViewSpacing;
  // Size for the imageView width and height.
  const CGFloat kImageViewWidthHeight;
  // Margins for the primary button.
  const CGFloat kButtonTitleHorizontalContentInset;
  const CGFloat kButtonTitleVerticalContentInset;
  // Button corner radius.
  const CGFloat kButtonCornerRadius;
  // Margins for the close button.
  const CGFloat kCloseButtonTrailingMargin;
  const CGFloat kCloseButtonTopMargin;
} PromoStyleValues;

const PromoStyleValues kStandardPromoStyle = {
    11.0,  // kStackViewVerticalPadding
    16.0,  // kStackViewTrailingMargin
    13.0,  // kContentStackViewSubViewSpacing
    13.0,  // kTextStackViewSubViewSpacing
    32.0,  // kImageViewWidthHeight
    12.0,  // kButtonTitleHorizontalContentInset
    8.0,   // kButtonTitleVerticalContentInset
    8.0,   // kButtonCornerRadius
    5.0,   // kCloseButtonTrailingMargin
    0.0,   // kCloseButtonTopMargin
};

const PromoStyleValues kCompactPromoStyle = {
    18.0,  // kStackViewVerticalPadding
    41.0,  // kStackViewTrailingMargin
    17.0,  // kContentStackViewSubViewSpacing
    4.0,   // kTextStackViewSubViewSpacing
    56.0,  // kImageViewWidthHeight
    0.0,   // kButtonTitleHorizontalContentInset
    0.0,   // kButtonTitleVerticalContentInset
    0.0,   // kButtonCornerRadius
    -9.0,  // kCloseButtonTrailingMargin
    9.0,   // kCloseButtonTopMargin
};

// UI Refresh Constants:
// Horizontal padding for label and buttons.
constexpr CGFloat kHorizontalPadding = 40;
// Horizontal spacing between stackView and cell contentView.
constexpr CGFloat kStackViewHorizontalPadding = 16.0;

// Non-profile icon background corner radius.
constexpr CGFloat kNonProfileIconCornerRadius = 14;
// Size for the close button width and height.
constexpr CGFloat kCloseButtonWidthHeight = 24;
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
// Constraints for the different layout styles.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* standardLayoutConstraints;
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* compactLayoutConstraints;
// Constraints for the image size.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* imageConstraints;
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
    // Title is hidden by default.
    _titleLabel.hidden = YES;

    // Create and setup informative text label.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.numberOfLines = 0;
    _textLabel.textAlignment = NSTextAlignmentCenter;
    _textLabel.lineBreakMode = NSLineBreakByWordWrapping;

    // Create and setup primary button.
    _primaryButton = [[UIButton alloc] init];
    [_primaryButton.titleLabel
        setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]];
    _primaryButton.titleLabel.adjustsFontSizeToFitWidth = YES;
    _primaryButton.titleLabel.minimumScaleFactor = 0.7;
    _primaryButton.accessibilityIdentifier = kSigninPromoPrimaryButtonId;
    _primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
    _primaryButton.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    [_primaryButton addTarget:self
                       action:@selector(onPrimaryButtonAction:)
             forControlEvents:UIControlEventTouchUpInside];
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

    _textVerticalStackView.axis = UILayoutConstraintAxisVertical;
    _textVerticalStackView.translatesAutoresizingMaskIntoConstraints = NO;

    _contentStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _imageView, _textVerticalStackView ]];
    _contentStackView.alignment = UIStackViewAlignmentCenter;
    _contentStackView.translatesAutoresizingMaskIntoConstraints = NO;

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

    // Constraints that apply to all styles.
    [NSLayoutConstraint activateConstraints:@[
      [_contentStackView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kStackViewHorizontalPadding],
      // Close button size constraints.
      [_closeButton.heightAnchor
          constraintEqualToConstant:kCloseButtonWidthHeight],
      [_closeButton.widthAnchor
          constraintEqualToConstant:kCloseButtonWidthHeight],
    ]];

    // Default layout style.
    _compactLayout = NO;
    [self updateLayoutForStyle];
    // Default mode.
    _mode = SigninPromoViewModeNoAccounts;
    [self activateNoAccountsMode];
  }
  return self;
}

#pragma mark - Public

- (CGFloat)horizontalPadding {
  return kHorizontalPadding;
}

- (void)setProfileImage:(UIImage*)image {
  DCHECK_NE(self.mode, SigninPromoViewModeNoAccounts);
  [self updateImageSizeForProfileImage:YES];
  DCHECK_EQ(kStandardPromoStyle.kImageViewWidthHeight, image.size.height);
  DCHECK_EQ(kStandardPromoStyle.kImageViewWidthHeight, image.size.width);
  self.imageView.image =
      CircularImageFromImage(image, kStandardPromoStyle.kImageViewWidthHeight);
  self.backgroundColor = nil;
  self.imageView.layer.cornerRadius = 0;
}

- (void)setNonProfileImage:(UIImage*)image {
  [self updateImageSizeForProfileImage:NO];
  CGFloat imageSize = self.compactLayout
                          ? kCompactPromoStyle.kImageViewWidthHeight
                          : kStandardPromoStyle.kImageViewWidthHeight;
  DCHECK_EQ(imageSize, image.size.width);
  DCHECK_EQ(imageSize, image.size.height);
  self.imageView.image = image;
  self.imageView.backgroundColor = [UIColor colorNamed:kSolidPrimaryColor];
  self.imageView.layer.cornerRadius = kNonProfileIconCornerRadius;
}

- (void)prepareForReuse {
  self.delegate = nil;
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

  if (self.mode == SigninPromoViewModeSigninWithAccount) {
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

#pragma mark - Setters

// Sets layout style and updates layout accordingly.
- (void)setCompactLayout:(BOOL)compactLayout {
  if (_compactLayout == compactLayout) {
    return;
  }
  _compactLayout = compactLayout;
  [self updateLayoutForStyle];
}

// Sets mode and updates promo accordingly.
- (void)setMode:(SigninPromoViewMode)mode {
  if (_mode == mode) {
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
}

#pragma mark - Getters

// Constraints specific to standard layout.
- (NSArray<NSLayoutConstraint*>*)standardLayoutConstraints {
  if (!_standardLayoutConstraints) {
    _standardLayoutConstraints = @[
      // Content padding.
      [self.contentStackView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kStandardPromoStyle
                                      .kStackViewVerticalPadding],
      [self.contentStackView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-kStandardPromoStyle
                                       .kStackViewVerticalPadding],
      [self.contentStackView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kStandardPromoStyle
                                       .kStackViewTrailingMargin],
      [self.closeButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:kStandardPromoStyle
                                      .kCloseButtonTrailingMargin],
      [self.closeButton.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kStandardPromoStyle.kCloseButtonTopMargin],
    ];
  }
  return _standardLayoutConstraints;
}

// Constraints specific to compact layout.
- (NSArray<NSLayoutConstraint*>*)compactLayoutConstraints {
  if (!_compactLayoutConstraints) {
    _compactLayoutConstraints = @[
      // Content padding.
      [self.contentStackView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kCompactPromoStyle.kStackViewVerticalPadding],
      [self.contentStackView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-kCompactPromoStyle
                                       .kStackViewVerticalPadding],
      [self.contentStackView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kCompactPromoStyle.kStackViewTrailingMargin],
      [self.closeButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:kCompactPromoStyle
                                      .kCloseButtonTrailingMargin],
      [self.closeButton.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kCompactPromoStyle.kCloseButtonTopMargin],
    ];
  }
  return _compactLayoutConstraints;
}

#pragma mark - Private

// Updates layout for current layout style.
- (void)updateLayoutForStyle {
  if (self.compactLayout) {
    // Lays out content for compact view.
    self.contentStackView.axis = UILayoutConstraintAxisHorizontal;
    self.contentStackView.spacing =
        kCompactPromoStyle.kContentStackViewSubViewSpacing;
    self.textVerticalStackView.alignment = UIStackViewAlignmentLeading;
    self.textVerticalStackView.spacing =
        kCompactPromoStyle.kTextStackViewSubViewSpacing;
    self.textLabel.textAlignment = NSTextAlignmentNatural;
    self.secondaryButton.hidden = YES;

    // Configures fonts for compact layout.
    self.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    self.titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    self.textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCallout];
    self.textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

    // In the compact layout, the primary button is plain.
    [self.primaryButton setTitleColor:[UIColor colorNamed:kBlueColor]
                             forState:UIControlStateNormal];
    self.primaryButton.backgroundColor = nil;
    self.primaryButton.layer.cornerRadius =
        kCompactPromoStyle.kButtonCornerRadius;
    self.primaryButton.clipsToBounds = NO;
    self.primaryButton.contentEdgeInsets =
        UIEdgeInsetsMake(kCompactPromoStyle.kButtonTitleVerticalContentInset,
                         kCompactPromoStyle.kButtonTitleHorizontalContentInset,
                         kCompactPromoStyle.kButtonTitleVerticalContentInset,
                         kCompactPromoStyle.kButtonTitleHorizontalContentInset);

    // Applies constraints for compact layout.
    [NSLayoutConstraint deactivateConstraints:self.standardLayoutConstraints];
    [NSLayoutConstraint activateConstraints:self.compactLayoutConstraints];
  } else {
    // Lays out content vertically for standard view.
    self.contentStackView.axis = UILayoutConstraintAxisVertical;
    self.contentStackView.spacing =
        kStandardPromoStyle.kContentStackViewSubViewSpacing;
    self.textVerticalStackView.alignment = UIStackViewAlignmentCenter;
    self.textVerticalStackView.spacing =
        kStandardPromoStyle.kTextStackViewSubViewSpacing;
    self.textLabel.textAlignment = NSTextAlignmentCenter;

    // Configures fonts for standard layout.
    self.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleTitle3];
    self.titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    self.textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    self.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

    // In the standard layout, the button has a background.
    [self.primaryButton setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                             forState:UIControlStateNormal];
    self.primaryButton.backgroundColor = [UIColor colorNamed:kBlueColor];
    self.primaryButton.layer.cornerRadius =
        kStandardPromoStyle.kButtonCornerRadius;
    self.primaryButton.clipsToBounds = YES;
    self.primaryButton.contentEdgeInsets = UIEdgeInsetsMake(
        kStandardPromoStyle.kButtonTitleVerticalContentInset,
        kStandardPromoStyle.kButtonTitleHorizontalContentInset,
        kStandardPromoStyle.kButtonTitleVerticalContentInset,
        kStandardPromoStyle.kButtonTitleHorizontalContentInset);

    // Applies constraints for standard layout.
    [NSLayoutConstraint deactivateConstraints:self.compactLayoutConstraints];
    [NSLayoutConstraint activateConstraints:self.standardLayoutConstraints];
  }
  [self layoutIfNeeded];
}

// Updates image size constraints based on if it is a profile avatar.
- (void)updateImageSizeForProfileImage:(BOOL)isProfileImage {
  CGFloat imageSize;
  if (self.compactLayout && !isProfileImage) {
    imageSize = kCompactPromoStyle.kImageViewWidthHeight;
  } else {
    imageSize = kStandardPromoStyle.kImageViewWidthHeight;
  }

  [NSLayoutConstraint deactivateConstraints:self.imageConstraints];
  self.imageConstraints = @[
    [self.imageView.heightAnchor constraintEqualToConstant:imageSize],
    [self.imageView.widthAnchor constraintEqualToConstant:imageSize],
  ];
  [NSLayoutConstraint activateConstraints:self.imageConstraints];
}

// Updates promo for no accounts mode.
- (void)activateNoAccountsMode {
  DCHECK_EQ(self.mode, SigninPromoViewModeNoAccounts);
  UIImage* logo = nil;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  logo = [UIImage imageNamed:@"signin_promo_logo_chrome_color"];
#else
  logo = [UIImage imageNamed:@"signin_promo_logo_chromium_color"];
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  DCHECK(logo);
  self.imageView.image = logo;
  self.secondaryButton.hidden = YES;
}

// Updates promo for sign-in with account mode.
- (void)activateSigninWithAccountMode {
  DCHECK_EQ(self.mode, SigninPromoViewModeSigninWithAccount);
  self.secondaryButton.hidden = NO;
}

// Updates promo for sync with account mode.
- (void)activateSyncWithPrimaryAccountMode {
  DCHECK_EQ(_mode, SigninPromoViewModeSyncWithPrimaryAccount);
  self.secondaryButton.hidden = YES;
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

// Handles primary button action based on `mode`.
- (void)onPrimaryButtonAction:(id)unused {
  switch (self.mode) {
    case SigninPromoViewModeNoAccounts:
      [self.delegate signinPromoViewDidTapSigninWithNewAccount:self];
      break;
    case SigninPromoViewModeSigninWithAccount:
    case SigninPromoViewModeSyncWithPrimaryAccount:
      [self.delegate signinPromoViewDidTapSigninWithDefaultAccount:self];
      break;
  }
}

// Handles secondary button action.
- (void)onSecondaryButtonAction:(id)unused {
  [self.delegate signinPromoViewDidTapSigninWithOtherAccount:self];
}

// Handles close button action.
- (void)onCloseButtonAction:(id)unused {
  [self.delegate signinPromoViewCloseButtonWasTapped:self];
}

@end
