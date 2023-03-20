// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"

#import "base/check_op.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "build/branding_buildflags.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_delegate.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef struct {
  // Vertical spacing between stackView and cell contentView.
  const CGFloat kStackViewTopPadding;
  const CGFloat kStackViewBottomPadding;
  // Trailing margin for content.
  const CGFloat kStackViewTrailingMargin;
  // Spacing within content stack view.
  const CGFloat kContentStackViewSubViewSpacing;
  // Spacing within text stack view.
  const CGFloat kTextStackViewSubViewSpacing;
  // Margins for the primary button.
  const CGFloat kButtonTitleHorizontalContentInset;
  const CGFloat kButtonTitleVerticalContentInset;
  // Button corner radius.
  const CGFloat kButtonCornerRadius;
  // Margins for the close button.
  const CGFloat kCloseButtonTrailingMargin;
  const CGFloat kCloseButtonTopMargin;
  const CGFloat kMainPromoSubViewSpacing;
  const CGFloat kButtonStackViewSubViewSpacing;
} PromoStyleValues;

const PromoStyleValues kStandardPromoStyle = {
    11.0,  // kStackViewTopPadding
    11.0,  // kStackViewBottomPadding
    16.0,  // kStackViewTrailingMargin
    13.0,  // kContentStackViewSubViewSpacing
    13.0,  // kTextStackViewSubViewSpacing
    12.0,  // kButtonTitleHorizontalContentInset
    8.0,   // kButtonTitleVerticalContentInset
    8.0,   // kButtonCornerRadius
    5.0,   // kCloseButtonTrailingMargin
    0.0,   // kCloseButtonTopMargin
    13.0,  // kMainPromoSubViewSpacing
    13.0,  // kButtonStackViewSubViewSpacing
};

// TODO(crbug.com/1331010): We may remove these styles if we don't launch them
// with the feed promo.
const PromoStyleValues kCompactVerticalStyle = {
    16.0,  // kStackViewTopPadding
    16.0,  // kStackViewBottomPadding
    19.0,  // kStackViewTrailingMargin
    10.0,  // kContentStackViewSubViewSpacing
    5.0,   // kTextStackViewSubViewSpacing
    42.0,  // kButtonTitleHorizontalContentInset
    9.0,   // kButtonTitleVerticalContentInset
    8.0,   // kButtonCornerRadius
    -8.0,  // kCloseButtonTrailingMargin
    8.0,   // kCloseButtonTopMargin
    12.0,  // kMainPromoSubViewSpacing
    5.0,   // kButtonStackViewSubViewSpacing
};

const PromoStyleValues kCompactHorizontalStyle = {
    20.0,  // kStackViewTopPadding
    14.0,  // kStackViewBottomPadding
    42.0,  // kStackViewTrailingMargin
    14.0,  // kContentStackViewSubViewSpacing
    0.0,   // kTextStackViewSubViewSpacing
    0.0,   // kButtonTitleHorizontalContentInset
    0.0,   // kButtonTitleVerticalContentInset
    0.0,   // kButtonCornerRadius
    -9.0,  // kCloseButtonTrailingMargin
    9.0,   // kCloseButtonTopMargin
    6.0,   // kMainPromoSubViewSpacing
    0.0,   // kButtonStackViewSubViewSpacing
};

const PromoStyleValues kTitledCompactPromoStyle = {
    18.0,  // kStackViewTopPadding
    18.0,  // kStackViewBottomPadding
    41.0,  // kStackViewTrailingMargin
    17.0,  // kContentStackViewSubViewSpacing
    4.0,   // kTextStackViewSubViewSpacing
    0.0,   // kButtonTitleHorizontalContentInset
    0.0,   // kButtonTitleVerticalContentInset
    0.0,   // kButtonCornerRadius
    -9.0,  // kCloseButtonTrailingMargin
    9.0,   // kCloseButtonTopMargin
    4.0,   // kMainPromoSubViewSpacing
    4.0,   // kButtonStackViewSubViewSpacing
};

// Horizontal padding for label and buttons.
constexpr CGFloat kHorizontalPadding = 40;
// Horizontal spacing between stackView and cell contentView.
constexpr CGFloat kStackViewHorizontalPadding = 16.0;
// Non-profile icon background corner radius.
constexpr CGFloat kNonProfileIconCornerRadius = 14;
// Size for the close button width and height.
constexpr CGFloat kCloseButtonWidthHeight = 24;
// Sizes of the signin promo image.
constexpr CGFloat kProfileImageHeightWidth = 32.0;
constexpr CGFloat kProfileImageCompactHeightWidth = 48.0;
constexpr CGFloat kNonProfileLogoImageCompactHeightWidth = 34.0;
constexpr CGFloat kNonProfileBackgroundImageCompactHeightWidth = 54.0;
constexpr CGFloat kNonProfileImageHeightWidth = 56.0;
// Size of the font for the headline.
constexpr CGFloat kSignInPromoHeadlineFontSize = 17.0;
// Constant for the size of the compact style text.
constexpr CGFloat kCompactStyleTextSize = 15.0;
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
// Contains all the text elements of the promo (title,body).
@property(nonatomic, strong) UIStackView* textVerticalStackView;
// Contains all the button elements of the promo.
@property(nonatomic, strong) UIStackView* buttonVerticalStackView;
// Parent Stack view that contains the `textVerticalStackView` and
// `buttonVerticalStackView` (Text, Buttons).
@property(nonatomic, strong) UIStackView* mainPromoStackView;

// Constraints for the different layout styles.
@property(nonatomic, weak)
    NSArray<NSLayoutConstraint*>* currentLayoutConstraints;
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* standardLayoutConstraints;
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* compactVerticalLayoutConstraints;
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* compactHorizontalLayoutConstraints;
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* compactTitledLayoutConstraints;
// Constraints for the image size.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* imageConstraints;
@end

@implementation SigninPromoView {
  signin_metrics::AccessPoint _accessPoint;
  // Activity indicator shown on top of the primary button.
  // See `startSignInSpinner` and `stopSignInSpinner`.
  UIActivityIndicatorView* _activityIndicatorView;
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
    // TODO(crbug.com/1418068): Simplify after minimum version required is >=
    // iOS 15.
    if (base::ios::IsRunningOnIOS15OrLater() &&
        IsUIButtonConfigurationEnabled()) {
      if (@available(iOS 15, *)) {
        UIButtonConfiguration* buttonConfiguration =
            [UIButtonConfiguration plainButtonConfiguration];
        _primaryButton = [UIButton buttonWithConfiguration:buttonConfiguration
                                             primaryAction:nil];
      }
    } else {
      _primaryButton = [[UIButton alloc] init];
    }

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
      _titleLabel,
      _textLabel,
    ]];

    _textVerticalStackView.axis = UILayoutConstraintAxisVertical;
    _textVerticalStackView.translatesAutoresizingMaskIntoConstraints = NO;

    // Separate the buttons from the text to custom-set the spacing between text
    // and buttons.
    _buttonVerticalStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _primaryButton, _secondaryButton ]];
    _buttonVerticalStackView.axis = UILayoutConstraintAxisVertical;
    _buttonVerticalStackView.translatesAutoresizingMaskIntoConstraints = NO;

    _mainPromoStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _textVerticalStackView, _buttonVerticalStackView
    ]];
    _mainPromoStackView.alignment = UIStackViewAlignmentCenter;
    _mainPromoStackView.axis = UILayoutConstraintAxisVertical;
    _mainPromoStackView.translatesAutoresizingMaskIntoConstraints = NO;

    _contentStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _imageView, _mainPromoStackView ]];
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
    _promoViewStyle = SigninPromoViewStyleStandard;
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
  switch (self.promoViewStyle) {
    case SigninPromoViewStyleStandard:
      [self updateImageWithSize:kProfileImageHeightWidth];
      break;
    case SigninPromoViewStyleCompactVertical:
    case SigninPromoViewStyleCompactHorizontal:
      [self updateImageWithSize:kProfileImageCompactHeightWidth];
      break;
    case SigninPromoViewStyleCompactTitled:
      // Compact Titled should not have a profile image, nor it should call
      // `setProfileImage:`.
      CHECK(NO);
      break;
  }
  DCHECK_EQ(kProfileImageHeightWidth, image.size.height);
  DCHECK_EQ(kProfileImageHeightWidth, image.size.width);
  self.imageView.image =
      CircularImageFromImage(image, kProfileImageHeightWidth);
  self.backgroundColor = nil;
  self.imageView.layer.cornerRadius = 0;
}

- (void)setNonProfileImage:(UIImage*)image {
  switch (self.promoViewStyle) {
    case SigninPromoViewStyleStandard:
      // Standard Style should not call `setNonProfileImage`.
      DCHECK(NO);
      break;
    case SigninPromoViewStyleCompactTitled:
      [self updateImageWithSize:kNonProfileImageHeightWidth];
      self.imageView.image = image;
      self.imageView.backgroundColor = [UIColor colorNamed:kSolidWhiteColor];
      self.imageView.layer.cornerRadius = kNonProfileIconCornerRadius;
      break;
    case SigninPromoViewStyleCompactVertical:
    case SigninPromoViewStyleCompactHorizontal: {
      [self updateImageWithSize:kNonProfileBackgroundImageCompactHeightWidth];
      // Declare a new image view to hold the non-profile image logo
      UIImageView* logoImageView = [[UIImageView alloc] init];
      logoImageView.image = image;
      self.imageView.image = nil;
      self.imageView.backgroundColor =
          [UIColor colorNamed:kPrimaryBackgroundColor];
      self.imageView.layer.cornerRadius = kNonProfileIconCornerRadius;

      logoImageView.translatesAutoresizingMaskIntoConstraints = NO;
      logoImageView.contentMode = UIViewContentModeScaleAspectFit;
      [NSLayoutConstraint activateConstraints:@[
        [logoImageView.heightAnchor
            constraintEqualToConstant:kNonProfileLogoImageCompactHeightWidth],
        [logoImageView.widthAnchor
            constraintEqualToConstant:kNonProfileLogoImageCompactHeightWidth],
      ]];
      // Add subview and constraints to current UIImageView which represents the
      // logo's solid background.
      [self.imageView addSubview:logoImageView];
      [NSLayoutConstraint activateConstraints:@[
        [logoImageView.centerXAnchor
            constraintEqualToAnchor:self.imageView.centerXAnchor],
        [logoImageView.centerYAnchor
            constraintEqualToAnchor:self.imageView.centerYAnchor]
      ]];
      break;
    }
  }
}

- (void)prepareForReuse {
  self.delegate = nil;
  if (_activityIndicatorView) {
    [self stopSignInSpinner];
  }
}

- (void)startSignInSpinner {
  if (_activityIndicatorView) {
    return;
  }
  self.primaryButton.titleLabel.alpha = 0;
  _activityIndicatorView = [[UIActivityIndicatorView alloc] init];
  _activityIndicatorView.color = UIColor.whiteColor;
  _activityIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_activityIndicatorView];
  [NSLayoutConstraint activateConstraints:@[
    [_activityIndicatorView.centerXAnchor
        constraintEqualToAnchor:self.primaryButton.centerXAnchor],
    [_activityIndicatorView.centerYAnchor
        constraintEqualToAnchor:self.primaryButton.centerYAnchor],
  ]];
  self.primaryButton.enabled = NO;
  self.secondaryButton.enabled = NO;
  self.closeButton.enabled = NO;
  [_activityIndicatorView startAnimating];
}

- (void)stopSignInSpinner {
  if (!_activityIndicatorView) {
    return;
  }
  self.primaryButton.titleLabel.alpha = 1.;
  [_activityIndicatorView removeFromSuperview];
  _activityIndicatorView = nil;
  self.primaryButton.enabled = YES;
  self.secondaryButton.enabled = YES;
  self.closeButton.enabled = YES;
}

#pragma mark - NSObject(Accessibility)

- (void)setAccessibilityLabel:(NSString*)accessibilityLabel {
  NOTREACHED();
}

- (NSString*)accessibilityLabel {
  return self.titleLabel.hidden
             ? [NSString
                   stringWithFormat:@"%@ %@", self.textLabel.text,
                                    [self.primaryButton
                                        titleForState:UIControlStateNormal]]
             : [NSString
                   stringWithFormat:@"%@. %@ %@", self.titleLabel.text,
                                    self.textLabel.text,
                                    [self.primaryButton
                                        titleForState:UIControlStateNormal]];
}

- (BOOL)accessibilityActivate {
  if (!self.primaryButton.enabled) {
    return NO;
  }
  [self accessibilityPrimaryAction:nil];
  return YES;
}

- (NSArray<UIAccessibilityCustomAction*>*)accessibilityCustomActions {
  NSMutableArray* actions = [NSMutableArray array];
  if (self.secondaryButton.enabled &&
      self.mode == SigninPromoViewModeSigninWithAccount) {
    NSString* secondaryActionName =
        [self.secondaryButton titleForState:UIControlStateNormal];
    UIAccessibilityCustomAction* secondaryCustomAction =
        [[UIAccessibilityCustomAction alloc]
            initWithName:secondaryActionName
                  target:self
                selector:@selector(accessibilitySecondaryAction:)];
    [actions addObject:secondaryCustomAction];
  }
  if (self.closeButton.enabled && !self.closeButton.hidden) {
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

// Sets promo style and updates layout accordingly.
- (void)setPromoViewStyle:(SigninPromoViewStyle)promoViewStyle {
  if (_promoViewStyle == promoViewStyle) {
    return;
  }
  _promoViewStyle = promoViewStyle;
  [self updateLayoutForStyle];
  [self layoutIfNeeded];
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
                         constant:kStandardPromoStyle.kStackViewTopPadding],
      [self.contentStackView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-kStandardPromoStyle.kStackViewBottomPadding],
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

// Constraints specific to the compact vertical layout.
- (NSArray<NSLayoutConstraint*>*)compactVerticalLayoutConstraints {
  if (!_compactVerticalLayoutConstraints) {
    _compactVerticalLayoutConstraints = @[
      // Content padding.
      [self.contentStackView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kCompactVerticalStyle.kStackViewTopPadding],
      [self.contentStackView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-kCompactVerticalStyle
                                       .kStackViewBottomPadding],
      [self.contentStackView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kCompactVerticalStyle
                                       .kStackViewTrailingMargin],
      [self.closeButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:kCompactVerticalStyle
                                      .kCloseButtonTrailingMargin],
      [self.closeButton.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kCompactVerticalStyle.kCloseButtonTopMargin],
    ];
  }
  return _compactVerticalLayoutConstraints;
}

// Constraints specific to compact horitzontal layout.
- (NSArray<NSLayoutConstraint*>*)compactHorizontalLayoutConstraints {
  if (!_compactHorizontalLayoutConstraints) {
    _compactHorizontalLayoutConstraints = @[
      // Content padding.
      [self.contentStackView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kCompactHorizontalStyle.kStackViewTopPadding],
      [self.contentStackView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-kCompactHorizontalStyle
                                       .kStackViewBottomPadding],
      [self.contentStackView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kCompactHorizontalStyle
                                       .kStackViewTrailingMargin],
      [self.closeButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:kCompactHorizontalStyle
                                      .kCloseButtonTrailingMargin],
      [self.closeButton.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kCompactHorizontalStyle
                                      .kCloseButtonTopMargin],
    ];
  }
  return _compactHorizontalLayoutConstraints;
}

// Constraints specific to titled compact layout.
- (NSArray<NSLayoutConstraint*>*)compactTitledLayoutConstraints {
  if (!_compactTitledLayoutConstraints) {
    _compactTitledLayoutConstraints = @[
      // Content padding.
      [self.contentStackView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kTitledCompactPromoStyle
                                      .kStackViewTopPadding],
      [self.contentStackView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-kTitledCompactPromoStyle
                                       .kStackViewBottomPadding],
      [self.contentStackView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kTitledCompactPromoStyle
                                       .kStackViewTrailingMargin],
      [self.closeButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:kTitledCompactPromoStyle
                                      .kCloseButtonTrailingMargin],
      [self.closeButton.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kTitledCompactPromoStyle
                                      .kCloseButtonTopMargin],
    ];
  }
  return _compactTitledLayoutConstraints;
}

#pragma mark - Private

// Updates layout for current layout style.
- (void)updateLayoutForStyle {
  NSArray<NSLayoutConstraint*>* constraintsToActivate;
  switch (self.promoViewStyle) {
    case SigninPromoViewStyleStandard: {
      // Lays out content vertically for standard view.
      self.buttonVerticalStackView.axis = UILayoutConstraintAxisVertical;
      self.buttonVerticalStackView.spacing =
          kStandardPromoStyle.kButtonStackViewSubViewSpacing;
      self.mainPromoStackView.spacing =
          kStandardPromoStyle.kMainPromoSubViewSpacing;
      self.contentStackView.axis = UILayoutConstraintAxisVertical;
      self.contentStackView.spacing =
          kStandardPromoStyle.kContentStackViewSubViewSpacing;
      self.textVerticalStackView.alignment = UIStackViewAlignmentCenter;
      self.textVerticalStackView.spacing =
          kStandardPromoStyle.kTextStackViewSubViewSpacing;
      self.textLabel.textAlignment = NSTextAlignmentCenter;
      self.secondaryButton.hidden = NO;

      // Configures fonts for standard layout.
      self.titleLabel.font =
          [UIFont preferredFontForTextStyle:UIFontTextStyleTitle3];
      self.titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
      self.textLabel.font =
          [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
      self.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

      // In the standard layout, the button has a background.
      [self.primaryButton
          setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
               forState:UIControlStateNormal];
      self.primaryButton.backgroundColor = [UIColor colorNamed:kBlueColor];
      self.primaryButton.layer.cornerRadius =
          kStandardPromoStyle.kButtonCornerRadius;
      self.primaryButton.clipsToBounds = YES;

      // TODO(crbug.com/1418068): Simplify after minimum version required is >=
      // iOS 15.
      if (base::ios::IsRunningOnIOS15OrLater() &&
          IsUIButtonConfigurationEnabled()) {
        if (@available(iOS 15, *)) {
          self.primaryButton.configuration.contentInsets =
              NSDirectionalEdgeInsetsMake(
                  kStandardPromoStyle.kButtonTitleVerticalContentInset,
                  kStandardPromoStyle.kButtonTitleHorizontalContentInset,
                  kStandardPromoStyle.kButtonTitleVerticalContentInset,
                  kStandardPromoStyle.kButtonTitleHorizontalContentInset);
        }
      } else {
        UIEdgeInsets contentEdgeInsets = UIEdgeInsetsMake(
            kStandardPromoStyle.kButtonTitleVerticalContentInset,
            kStandardPromoStyle.kButtonTitleHorizontalContentInset,
            kStandardPromoStyle.kButtonTitleVerticalContentInset,
            kStandardPromoStyle.kButtonTitleHorizontalContentInset);
        SetContentEdgeInsets(self.primaryButton, contentEdgeInsets);
      }

      constraintsToActivate = self.standardLayoutConstraints;
      break;
    }
    case SigninPromoViewStyleCompactTitled: {
      // Lays out content for titled compact view.
      self.buttonVerticalStackView.alignment = UIStackViewAlignmentLeading;
      self.buttonVerticalStackView.spacing =
          kTitledCompactPromoStyle.kButtonStackViewSubViewSpacing;
      self.mainPromoStackView.alignment = UIStackViewAlignmentLeading;
      self.mainPromoStackView.spacing =
          kTitledCompactPromoStyle.kMainPromoSubViewSpacing;
      self.contentStackView.axis = UILayoutConstraintAxisHorizontal;
      self.contentStackView.spacing =
          kTitledCompactPromoStyle.kContentStackViewSubViewSpacing;
      self.textVerticalStackView.alignment = UIStackViewAlignmentLeading;
      self.textVerticalStackView.spacing =
          kTitledCompactPromoStyle.kTextStackViewSubViewSpacing;
      self.textLabel.textAlignment = NSTextAlignmentNatural;
      self.secondaryButton.hidden = YES;

      // Configures fonts for titled compact layout.
      self.titleLabel.font =
          [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
      self.titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
      self.textLabel.font =
          [UIFont preferredFontForTextStyle:UIFontTextStyleCallout];
      self.textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

      // In the titled compact layout, the primary button is plain.
      [self.primaryButton setTitleColor:[UIColor colorNamed:kBlueColor]
                               forState:UIControlStateNormal];
      self.primaryButton.backgroundColor = nil;
      self.primaryButton.layer.cornerRadius =
          kTitledCompactPromoStyle.kButtonCornerRadius;
      self.primaryButton.clipsToBounds = NO;

      // TODO(crbug.com/1418068): Simplify after minimum version required is >=
      // iOS 15.
      if (base::ios::IsRunningOnIOS15OrLater() &&
          IsUIButtonConfigurationEnabled()) {
        if (@available(iOS 15, *)) {
          self.primaryButton.configuration.contentInsets =
              NSDirectionalEdgeInsetsMake(
                  kTitledCompactPromoStyle.kButtonTitleVerticalContentInset,
                  kTitledCompactPromoStyle.kButtonTitleHorizontalContentInset,
                  kTitledCompactPromoStyle.kButtonTitleVerticalContentInset,
                  kTitledCompactPromoStyle.kButtonTitleHorizontalContentInset);
        }
      } else {
        UIEdgeInsets contentEdgeInsets = UIEdgeInsetsMake(
            kTitledCompactPromoStyle.kButtonTitleVerticalContentInset,
            kTitledCompactPromoStyle.kButtonTitleHorizontalContentInset,
            kTitledCompactPromoStyle.kButtonTitleVerticalContentInset,
            kTitledCompactPromoStyle.kButtonTitleHorizontalContentInset);
        SetContentEdgeInsets(self.primaryButton, contentEdgeInsets);
      }

      constraintsToActivate = self.compactTitledLayoutConstraints;
      break;
    }
    case SigninPromoViewStyleCompactHorizontal: {
      // Lays out content for the horizontal compact view.
      self.buttonVerticalStackView.alignment = UIStackViewAlignmentLeading;
      self.buttonVerticalStackView.spacing =
          kCompactHorizontalStyle.kButtonStackViewSubViewSpacing;
      self.mainPromoStackView.alignment = UIStackViewAlignmentLeading;
      self.mainPromoStackView.spacing =
          kCompactHorizontalStyle.kMainPromoSubViewSpacing;
      self.contentStackView.alignment = UIStackViewAlignmentTop;
      self.contentStackView.axis = UILayoutConstraintAxisHorizontal;
      self.contentStackView.spacing =
          kCompactHorizontalStyle.kContentStackViewSubViewSpacing;
      self.textVerticalStackView.alignment = UIStackViewAlignmentLeading;
      self.textVerticalStackView.spacing =
          kCompactHorizontalStyle.kTextStackViewSubViewSpacing;
      self.textLabel.textAlignment = NSTextAlignmentNatural;
      self.secondaryButton.hidden = YES;

      // Configures fonts for the compact horizontal layout.
      self.textLabel.font =
          [[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
              fontWithSize:kCompactStyleTextSize];
      self.textLabel.textColor = [UIColor colorNamed:kGrey800Color];

      // In the Compact Horizontal style, the primary button is plain.
      [self.primaryButton setTitleColor:[UIColor colorNamed:kBlueColor]
                               forState:UIControlStateNormal];
      self.primaryButton.backgroundColor = nil;
      self.primaryButton.layer.cornerRadius =
          kCompactHorizontalStyle.kButtonCornerRadius;
      self.primaryButton.clipsToBounds = NO;

      // TODO(crbug.com/1418068): Simplify after minimum version required is >=
      // iOS 15.
      if (base::ios::IsRunningOnIOS15OrLater() &&
          IsUIButtonConfigurationEnabled()) {
        if (@available(iOS 15, *)) {
          self.primaryButton.configuration.contentInsets =
              NSDirectionalEdgeInsetsMake(
                  kCompactHorizontalStyle.kButtonTitleVerticalContentInset,
                  kCompactHorizontalStyle.kButtonTitleHorizontalContentInset,
                  kCompactHorizontalStyle.kButtonTitleVerticalContentInset,
                  kCompactHorizontalStyle.kButtonTitleHorizontalContentInset);
        }
      } else {
        UIEdgeInsets contentEdgeInsets = UIEdgeInsetsMake(
            kCompactHorizontalStyle.kButtonTitleVerticalContentInset,
            kCompactHorizontalStyle.kButtonTitleHorizontalContentInset,
            kCompactHorizontalStyle.kButtonTitleVerticalContentInset,
            kCompactHorizontalStyle.kButtonTitleHorizontalContentInset);
        SetContentEdgeInsets(self.primaryButton, contentEdgeInsets);
      }

      constraintsToActivate = self.compactHorizontalLayoutConstraints;
      break;
    }
    case SigninPromoViewStyleCompactVertical: {
      self.contentStackView.axis = UILayoutConstraintAxisVertical;
      self.contentStackView.spacing =
          kCompactVerticalStyle.kContentStackViewSubViewSpacing;
      self.textVerticalStackView.alignment = UIStackViewAlignmentCenter;
      self.textVerticalStackView.spacing =
          kCompactVerticalStyle.kTextStackViewSubViewSpacing;
      self.buttonVerticalStackView.spacing =
          kCompactVerticalStyle.kButtonStackViewSubViewSpacing;
      self.mainPromoStackView.spacing =
          kCompactVerticalStyle.kMainPromoSubViewSpacing;
      self.textLabel.textAlignment = NSTextAlignmentCenter;
      self.secondaryButton.hidden = YES;
      self.imageView.hidden = NO;

      self.textLabel.font =
          [[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
              fontWithSize:kCompactStyleTextSize];
      self.textLabel.textColor = [UIColor colorNamed:kGrey800Color];

      [self.primaryButton setTitleColor:[UIColor colorNamed:kBlueColor]
                               forState:UIControlStateNormal];
      self.primaryButton.titleLabel.font =
          [[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
              fontWithSize:kSignInPromoHeadlineFontSize];
      self.primaryButton.backgroundColor =
          [UIColor colorNamed:kBackgroundColor];
      self.primaryButton.layer.cornerRadius =
          kCompactVerticalStyle.kButtonCornerRadius;
      self.primaryButton.clipsToBounds = YES;

      // TODO(crbug.com/1418068): Simplify after minimum version required is >=
      // iOS 15.
      if (base::ios::IsRunningOnIOS15OrLater() &&
          IsUIButtonConfigurationEnabled()) {
        if (@available(iOS 15, *)) {
          self.primaryButton.configuration.contentInsets =
              NSDirectionalEdgeInsetsMake(
                  kCompactVerticalStyle.kButtonTitleVerticalContentInset,
                  kCompactVerticalStyle.kButtonTitleHorizontalContentInset,
                  kCompactVerticalStyle.kButtonTitleVerticalContentInset,
                  kCompactVerticalStyle.kButtonTitleHorizontalContentInset);
        }
      } else {
        UIEdgeInsets contentEdgeInsets = UIEdgeInsetsMake(
            kCompactVerticalStyle.kButtonTitleVerticalContentInset,
            kCompactVerticalStyle.kButtonTitleHorizontalContentInset,
            kCompactVerticalStyle.kButtonTitleVerticalContentInset,
            kCompactVerticalStyle.kButtonTitleHorizontalContentInset);
        SetContentEdgeInsets(self.primaryButton, contentEdgeInsets);
      }

      constraintsToActivate = self.compactVerticalLayoutConstraints;
      break;
    }
  }
  // Removes previous constraints and activates new ones.
  [NSLayoutConstraint deactivateConstraints:self.currentLayoutConstraints];
  self.currentLayoutConstraints = constraintsToActivate;
  [NSLayoutConstraint activateConstraints:self.currentLayoutConstraints];
}

// Updates image size constraints based on if it is a profile avatar.
- (void)updateImageWithSize:(CGFloat)imageSize {
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
  DCHECK(self.primaryButton.enabled);
  [self.primaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];
}

- (void)accessibilitySecondaryAction:(id)unused {
  DCHECK(self.secondaryButton.enabled);
  [self.secondaryButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
}

- (void)accessibilityCloseAction:(id)unused {
  DCHECK(self.closeButton.enabled);
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
