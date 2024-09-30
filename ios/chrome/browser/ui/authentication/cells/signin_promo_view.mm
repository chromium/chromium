// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/ios/ios_util.h"
#import "base/notreached.h"
#import "build/branding_buildflags.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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

const PromoStyleValues kCompactVerticalStyle = {
    16.0,  // kStackViewTopPadding
    16.0,  // kStackViewBottomPadding
    19.0,  // kStackViewTrailingMargin
    10.0,  // kContentStackViewSubViewSpacing
    5.0,   // kTextStackViewSubViewSpacing
    42.0,  // kButtonTitleHorizontalContentInset
    9.0,   // kButtonTitleVerticalContentInset
    8.0,   // kButtonCornerRadius
    -9.0,  // kCloseButtonTrailingMargin
    9.0,   // kCloseButtonTopMargin
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
    3.0,   // kButtonTitleVerticalContentInset
    0.0,   // kButtonCornerRadius
    -9.0,  // kCloseButtonTrailingMargin
    9.0,   // kCloseButtonTopMargin
    6.0,   // kMainPromoSubViewSpacing
    0.0,   // kButtonStackViewSubViewSpacing
};

// Horizontal padding for label and buttons.
constexpr CGFloat kHorizontalPadding = 40;
// Horizontal spacing between stackView and cell contentView.
constexpr CGFloat kStackViewHorizontalPadding = 16.0;
// Non-profile icon background corner radius.
constexpr CGFloat kNonProfileIconCornerRadius = 14;
// Size for the close button width and height.
constexpr CGFloat kCloseButtonWidthHeight = 16;
// Sizes of the signin promo image.
constexpr CGFloat kProfileImageHeightWidth = 32.0;
constexpr CGFloat kProfileImageCompactHeightWidth = 48.0;
constexpr CGFloat kNonProfileLogoImageCompactHeightWidth = 34.0;
constexpr CGFloat kNonProfileBackgroundImageCompactHeightWidth = 54.0;
}

@interface SigninPromoView ()
// Re-declare as readwrite.
@property(nonatomic, strong, readwrite) UIImageView* imageView;
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

    // Create and setup informative text label.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.numberOfLines = 0;
    _textLabel.textAlignment = NSTextAlignmentCenter;
    _textLabel.lineBreakMode = NSLineBreakByWordWrapping;

    // Create and setup primary button.
    _primaryButton = [[UIButton alloc] init];
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.titleLineBreakMode = NSLineBreakByTruncatingTail;
    _primaryButton.configuration = buttonConfiguration;

    _primaryButton.accessibilityIdentifier = kSigninPromoPrimaryButtonId;
    _primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
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
    UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
        configurationWithPointSize:kCloseButtonWidthHeight
                            weight:UIImageSymbolWeightSemibold];
    UIImage* closeButtonImage =
        DefaultSymbolWithConfiguration(@"xmark", config);
    [_closeButton setImage:closeButtonImage forState:UIControlStateNormal];
    _closeButton.tintColor = [UIColor colorNamed:kTextTertiaryColor];
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
    case SigninPromoViewStyleCompact:
      [self updateImageWithSize:kProfileImageCompactHeightWidth];
      break;
    case SigninPromoViewStyleOnlyButton:
      // This style has no image.
      NOTREACHED();
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
    case SigninPromoViewStyleCompact: {
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
    case SigninPromoViewStyleOnlyButton:
      // This style has no image.
      NOTREACHED();
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
  _activityIndicatorView.color = [UIColor colorNamed:kSolidButtonTextColor];
  _activityIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
  _activityIndicatorView.accessibilityIdentifier =
      kSigninPromoActivityIndicatorId;
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

// Configures primary button with a standard font.
- (void)configurePrimaryButtonWithTitle:(NSString*)title {
  CHECK_GT(title.length, 0ul, base::NotFatalUntil::M135);
  // Declaring variables that are used throughout different switch cases.
  UIFont* font;
  NSAttributedString* attributedTitle;
  NSDictionary* attributes;
  UIButtonConfiguration* buttonConfiguration = self.primaryButton.configuration;

  // Customize UIButton based on SigninPromoViewStyle.
  switch (self.promoViewStyle) {
    case SigninPromoViewStyleCompact:
      font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
      attributes = @{NSFontAttributeName : font};
      attributedTitle = [[NSAttributedString alloc] initWithString:title
                                                        attributes:attributes];
      buttonConfiguration.attributedTitle = attributedTitle;
      break;
    case SigninPromoViewStyleStandard:
    case SigninPromoViewStyleOnlyButton:
      font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
      attributes = @{NSFontAttributeName : font};
      attributedTitle = [[NSAttributedString alloc] initWithString:title
                                                        attributes:attributes];
      buttonConfiguration.attributedTitle = attributedTitle;
      break;
  }
  self.primaryButton.configuration = buttonConfiguration;
}

#pragma mark - NSObject(Accessibility)

- (void)setAccessibilityLabel:(NSString*)accessibilityLabel {
  NOTREACHED_IN_MIGRATION();
}

- (NSString*)accessibilityLabel {
  return [NSString stringWithFormat:@"%@ %@", self.textLabel.text,
                                    [self primaryButtonTitle]];
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

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  // The name for Voice Control includes only
  // `self.primaryButton.titleLabel.text`.
  NSString* buttonTitle = [self primaryButtonTitle];
  if (!buttonTitle) {
    // TODO(crbug.com/365995361): At M135, this `if` can be removed.
    // Before M135, the CHECK in `-[SigninPromoView primaryButtonTitle]` is
    // non fatal if the title was not set. So to avoid a fatal exception,
    // this `if` is required.
    return @[];
  }
  return @[ buttonTitle ];
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
    case SigninPromoViewModeSignedInWithPrimaryAccount:
      [self activateSignedInWithPrimaryAccountMode];
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

#pragma mark - Private

// Returns the primary button title.
- (NSString*)primaryButtonTitle {
  NSString* buttonTitle = self.primaryButton.configuration.title;
  // The primary button should always be set.
  CHECK_GT(buttonTitle.length, 0ul, base::NotFatalUntil::M135);
  return buttonTitle;
}

// Updates layout for current layout style.
- (void)updateLayoutForStyle {
  NSArray<NSLayoutConstraint*>* constraintsToActivate;
  switch (self.promoViewStyle) {
    case SigninPromoViewStyleStandard: {
      // Lays out content vertically for standard view.
      self.buttonVerticalStackView.spacing =
          kStandardPromoStyle.kButtonStackViewSubViewSpacing;
      self.buttonVerticalStackView.alignment = UIStackViewAlignmentFill;
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
      self.textLabel.font =
          [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
      self.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

      // In the standard layout, the button has a background.
      self.primaryButton.backgroundColor = [UIColor colorNamed:kBlueColor];
      self.primaryButton.layer.cornerRadius =
          kStandardPromoStyle.kButtonCornerRadius;
      self.primaryButton.clipsToBounds = YES;

      UIButtonConfiguration* buttonConfiguration =
          self.primaryButton.configuration;
      buttonConfiguration.baseForegroundColor =
          [UIColor colorNamed:kSolidButtonTextColor];
      buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
          kStandardPromoStyle.kButtonTitleVerticalContentInset,
          kStandardPromoStyle.kButtonTitleHorizontalContentInset,
          kStandardPromoStyle.kButtonTitleVerticalContentInset,
          kStandardPromoStyle.kButtonTitleHorizontalContentInset);
      self.primaryButton.configuration = buttonConfiguration;
      constraintsToActivate = self.standardLayoutConstraints;
      break;
    }
    case SigninPromoViewStyleCompact: {
      self.buttonVerticalStackView.alignment = UIStackViewAlignmentFill;
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
          [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      self.textLabel.textColor = [UIColor colorNamed:kGrey800Color];
      self.primaryButton.backgroundColor =
          [UIColor colorNamed:kBackgroundColor];
      self.primaryButton.layer.cornerRadius =
          kCompactVerticalStyle.kButtonCornerRadius;
      self.primaryButton.clipsToBounds = YES;

      UIButtonConfiguration* buttonConfiguration =
          self.primaryButton.configuration;
      buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlueColor];
      buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
          kCompactVerticalStyle.kButtonTitleVerticalContentInset,
          kCompactVerticalStyle.kButtonTitleHorizontalContentInset,
          kCompactVerticalStyle.kButtonTitleVerticalContentInset,
          kCompactVerticalStyle.kButtonTitleHorizontalContentInset);
      self.primaryButton.configuration = buttonConfiguration;
      constraintsToActivate = self.compactVerticalLayoutConstraints;
      break;
    }
    case SigninPromoViewStyleOnlyButton: {
      self.buttonVerticalStackView.alignment = UIStackViewAlignmentCenter;
      self.textVerticalStackView.hidden = YES;
      self.secondaryButton.hidden = YES;
      self.imageView.hidden = YES;
      // Configuring spacings and axis for the stack views isn't necessary,
      // there's only one element shown per stack anyway.

      // Constants and constraints are reused from the standard layout.
      self.primaryButton.backgroundColor = [UIColor colorNamed:kBlueColor];
      self.primaryButton.layer.cornerRadius =
          kStandardPromoStyle.kButtonCornerRadius;
      self.primaryButton.clipsToBounds = YES;

      UIButtonConfiguration* buttonConfiguration =
          self.primaryButton.configuration;
      buttonConfiguration.baseForegroundColor =
          [UIColor colorNamed:kSolidButtonTextColor];
      buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
          kStandardPromoStyle.kButtonTitleVerticalContentInset,
          kHorizontalPadding,
          kStandardPromoStyle.kButtonTitleVerticalContentInset,
          kHorizontalPadding);
      self.primaryButton.configuration = buttonConfiguration;
      constraintsToActivate = self.standardLayoutConstraints;
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
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* logo = [UIImage imageNamed:kChromeSigninPromoLogoImage];
#else
  UIImage* logo = [UIImage imageNamed:kChromiumSigninPromoLogoImage];
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  DCHECK(logo);
  self.imageView.image = logo;
  self.secondaryButton.hidden = YES;
}

// Updates promo for sign-in with account mode.
- (void)activateSigninWithAccountMode {
  DCHECK_EQ(self.mode, SigninPromoViewModeSigninWithAccount);
  self.secondaryButton.hidden = NO;
}

// Updates promo for a signed-in account mode.
- (void)activateSignedInWithPrimaryAccountMode {
  DCHECK_EQ(_mode, SigninPromoViewModeSignedInWithPrimaryAccount);
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
    case SigninPromoViewModeSignedInWithPrimaryAccount:
      [self.delegate signinPromoViewDidTapPrimaryButtonWithDefaultAccount:self];
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
