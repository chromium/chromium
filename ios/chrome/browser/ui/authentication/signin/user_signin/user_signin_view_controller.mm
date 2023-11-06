// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_view_controller.h"

#import <MaterialComponents/MaterialActivityIndicator.h>

#import "base/check_op.h"
#import "base/ios/ios_util.h"
#import "base/logging.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Button corner radius.
const CGFloat kButtonCornerRadius = 8;
// Gradient height.
const CGFloat kGradientHeight = 40.;
// Max size for the user consent view.
const CGFloat kUserConsentMaxSize = 600.;
// Image inset for the more button.
const CGFloat kImageInset = 16.0;

// Layout constants for buttons.
struct AuthenticationViewConstants {
  CGFloat PrimaryFontSize;
  CGFloat SecondaryFontSize;
  CGFloat ButtonHeight;
  CGFloat ButtonHorizontalPadding;
  CGFloat ButtonVerticalPadding;
  CGFloat ButtonTitleContentHorizontalInset;
  CGFloat ButtonTitleContentVerticalInset;
};

typedef NS_ENUM(NSUInteger, ActionButtonStyle) {
  PRIMARY_ACTION_STYLE,
  SECONDARY_ACTION_STYLE,
};

const AuthenticationViewConstants kCompactConstants = {
    24,  // PrimaryFontSize
    14,  // SecondaryFontSize
    36,  // ButtonHeight
    16,  // ButtonHorizontalPadding
    16,  // ButtonVerticalPadding
    16,  // ButtonTitleContentHorizontalInset
    8,   // ButtonTitleContentVerticalInset
};

const AuthenticationViewConstants kRegularConstants = {
    1.5 * kCompactConstants.PrimaryFontSize,
    1.5 * kCompactConstants.SecondaryFontSize,
    1.5 * kCompactConstants.ButtonHeight,
    32,  // ButtonHorizontalPadding
    32,  // ButtonVerticalPadding
    16,  // ButtonTitleContentInset
    16,  // ButtonTitleContentInset
};

// The style applied to a button type.
enum AuthenticationButtonType {
  AuthenticationButtonTypeMore,
  AuthenticationButtonTypeAddAccount,
  AuthenticationButtonTypeConfirmation,
};
}  // namespace

@interface UserSigninViewController ()

// Container view used to center vertically the user consent view between the
// top of the controller view and the top of the button view.
@property(nonatomic, strong) UIView* containerView;
// Activity indicator used to block the UI when a sign-in operation is in
// progress.
@property(nonatomic, strong) MDCActivityIndicator* activityIndicator;
// Button used to confirm the sign-in operation, e.g. "Yes I'm In".
@property(nonatomic, strong) UIButton* primaryActionButton;
// Button used to exit the sign-in operation without confirmation, e.g. "No
// Thanks", "Cancel".
@property(nonatomic, strong) UIButton* secondaryActionButton;
// Stack view that displays the skip and continue buttons.
@property(nonatomic, strong) UIStackView* actionButtonsView;
// Property that denotes whether the unified consent screen reached bottom has
// triggered.
@property(nonatomic, assign) BOOL hasUnifiedConsentScreenReachedBottom;
// Gradient used to hide text that is close to the bottom of the screen. This
// gives users the hint that there is more to scroll through.
@property(nonatomic, strong, readonly) GradientView* gradientView;
// Lists of constraints that need to be activated when the view is in
// compact size class.
@property(nonatomic, strong, readonly) NSArray* compactSizeClassConstraints;
// Lists of constraints that need to be activated when the view is in
// regular size class.
@property(nonatomic, strong, readonly) NSArray* regularSizeClassConstraints;
// Unified consent view controller embedded in this view controller.
@property(nonatomic, strong, readonly) UIViewController* embeddedViewController;

@end

@implementation UserSigninViewController

@synthesize gradientView = _gradientView;
@synthesize compactSizeClassConstraints = _compactSizeClassConstraints;
@synthesize regularSizeClassConstraints = _regularSizeClassConstraints;

#pragma mark - Public

- (instancetype)initWithEmbeddedViewController:
    (UIViewController*)embeddedViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    DCHECK(embeddedViewController);
    _embeddedViewController = embeddedViewController;
  }
  return self;
}

- (void)markUnifiedConsentScreenReachedBottom {
  // This is the first time the unified consent screen has reached the bottom.
  if (!self.hasUnifiedConsentScreenReachedBottom) {
    self.hasUnifiedConsentScreenReachedBottom = YES;
    [self updatePrimaryActionButtonStyle];
  }
}

- (void)updatePrimaryActionButtonStyle {
  if (![self.delegate unifiedConsentCoordinatorHasIdentity]) {
    UIButtonConfiguration* buttonConfiguration =
        self.primaryActionButton.configuration;
    buttonConfiguration.image = nil;
    buttonConfiguration.title =
        l10n_util::GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_ADD_ACCOUNT);
    self.primaryActionButton.configuration = buttonConfiguration;
    self.primaryActionButton.tag = AuthenticationButtonTypeAddAccount;
    self.primaryActionButton.accessibilityIdentifier =
        kAddAccountAccessibilityIdentifier;
  } else if (!self.hasUnifiedConsentScreenReachedBottom) {
    // User screen is smaller than the consent text. Display option to
    // auto-scroll to the bottom of the screen.
    self.primaryActionButton.tag = AuthenticationButtonTypeMore;
    self.primaryActionButton.accessibilityIdentifier =
        kMoreAccessibilityIdentifier;

    // Set button "more" down directional arrow image.
    UIImage* buttonImage = [[UIImage imageNamed:@"signin_confirmation_more"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      UIButtonConfiguration* buttonConfiguration =
          self.primaryActionButton.configuration;
      buttonConfiguration.image = buttonImage;
      buttonConfiguration.title = l10n_util::GetNSString(
          IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SCROLL_BUTTON);
      buttonConfiguration.imagePadding = kImageInset;
      self.primaryActionButton.configuration = buttonConfiguration;
  } else {
    UIButtonConfiguration* buttonConfiguration =
        self.primaryActionButton.configuration;
    buttonConfiguration.image = nil;
    buttonConfiguration.title =
        l10n_util::GetNSString(self.acceptSigninButtonStringId);
    self.primaryActionButton.configuration = buttonConfiguration;
    self.primaryActionButton.tag = AuthenticationButtonTypeConfirmation;
    self.primaryActionButton.accessibilityIdentifier =
        kConfirmationAccessibilityIdentifier;
  }

  // Apply SECONDARY_ACTION_STYLE if the user has accounts on their device and
  // have not scrolled to the bottom of the consent screen.
  ActionButtonStyle style =
      [self.delegate unifiedConsentCoordinatorHasIdentity] &&
              !self.hasUnifiedConsentScreenReachedBottom
          ? SECONDARY_ACTION_STYLE
          : PRIMARY_ACTION_STYLE;
  [self updateButtonStyleWithButton:self.primaryActionButton style:style];
}

- (NSUInteger)supportedInterfaceOrientations {
  return (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
             ? [super supportedInterfaceOrientations]
             : UIInterfaceOrientationMaskPortrait;
}

- (void)signinWillStart {
  self.primaryActionButton.enabled = NO;
  [self startAnimatingActivityIndicator];
}

- (void)signinDidStop {
  self.primaryActionButton.enabled = YES;
  [self stopAnimatingActivityIndicator];
}

#pragma mark - AuthenticationFlowDelegate

- (void)didPresentDialog {
  [self.activityIndicator stopAnimating];
}

- (void)didDismissDialog {
  [self.activityIndicator startAnimating];
}

#pragma mark - MDCActivityIndicator

- (void)startAnimatingActivityIndicator {
  [self addActivityIndicator];
  [self.activityIndicator startAnimating];
}

- (void)stopAnimatingActivityIndicator {
  [self.activityIndicator stopAnimating];
  [self.activityIndicator removeFromSuperview];
  self.activityIndicator = nil;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  DCHECK(self.embeddedViewController);
  self.view.backgroundColor = self.systemBackgroundColor;

  self.containerView = [[UIView alloc] init];
  self.containerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.containerView];

  UIButtonConfiguration* primaryButtonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  self.primaryActionButton =
      [UIButton buttonWithConfiguration:primaryButtonConfiguration
                          primaryAction:nil];

  [self.primaryActionButton addTarget:self
                               action:@selector(onPrimaryActionButtonPressed:)
                     forControlEvents:UIControlEventTouchUpInside];
  [self updatePrimaryActionButtonStyle];
  self.primaryActionButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIButtonConfiguration* secondaryButtonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  self.secondaryActionButton =
      [UIButton buttonWithConfiguration:secondaryButtonConfiguration
                          primaryAction:nil];

  [self.secondaryActionButton
             addTarget:self
                action:@selector(onSecondaryActionButtonPressed:)
      forControlEvents:UIControlEventTouchUpInside];
  [self updateSecondaryButtonStyle];
  self.secondaryActionButton.translatesAutoresizingMaskIntoConstraints = NO;

  self.actionButtonsView = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.secondaryActionButton, self.primaryActionButton
  ]];
  self.actionButtonsView.distribution = UIStackViewDistributionEqualCentering;
  self.actionButtonsView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.actionButtonsView];

  self.embeddedViewController.view.translatesAutoresizingMaskIntoConstraints =
      NO;
  [self addChildViewController:self.embeddedViewController];
  [self.containerView addSubview:self.embeddedViewController.view];
  [self.embeddedViewController didMoveToParentViewController:self];

  self.gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.containerView addSubview:self.gradientView];

  [NSLayoutConstraint activateConstraints:@[
    // Note that the bottom constraint of the container view and
    // `embeddedViewController.view` is dependent on the selected
    // Accessibility options in Settings, e.g. text size. These constraints
    // are computed in `setAccessibilityLayoutConstraints`.
    [self.containerView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.containerView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.containerView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    // The gradient view needs to be attatched to the bottom of the user
    // consent view which contains the scroll view.
    [self.gradientView.bottomAnchor
        constraintEqualToAnchor:self.embeddedViewController.view.bottomAnchor],
    [self.gradientView.leadingAnchor
        constraintEqualToAnchor:self.embeddedViewController.view.leadingAnchor],
    [self.gradientView.trailingAnchor
        constraintEqualToAnchor:self.embeddedViewController.view
                                    .trailingAnchor],
    [self.gradientView.heightAnchor constraintEqualToConstant:kGradientHeight],
  ]];

  [self setAccessibilityLayoutConstraints];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self setAccessibilityLayoutConstraints];
}

#pragma mark - Constraints

// Generate default constraints based on the constants.
- (NSMutableArray*)generateConstraintsWithConstants:
    (AuthenticationViewConstants)constants {
  NSMutableArray* constraints = [NSMutableArray array];
  [constraints addObjectsFromArray:@[
    [self.view.safeAreaLayoutGuide.trailingAnchor
        constraintEqualToAnchor:self.actionButtonsView.trailingAnchor
                       constant:constants.ButtonHorizontalPadding],
    [self.view.safeAreaLayoutGuide.leadingAnchor
        constraintEqualToAnchor:self.actionButtonsView.leadingAnchor
                       constant:-constants.ButtonHorizontalPadding],
    [self.view.safeAreaLayoutGuide.bottomAnchor
        constraintEqualToAnchor:self.actionButtonsView.bottomAnchor
                       constant:constants.ButtonVerticalPadding]
  ]];
  return constraints;
}

// Sets the layout constraints depending on the selected text size in the
// accessibility Settings.
- (void)setAccessibilityLayoutConstraints {
  BOOL isRegularSizeClass = IsRegularXRegularSizeClass(self.traitCollection);
  UIFontTextStyle fontStyle;
  if (isRegularSizeClass) {
    [NSLayoutConstraint deactivateConstraints:self.compactSizeClassConstraints];
    [NSLayoutConstraint activateConstraints:self.regularSizeClassConstraints];
    fontStyle = UIFontTextStyleTitle2;
  } else {
    [NSLayoutConstraint deactivateConstraints:self.regularSizeClassConstraints];
    [NSLayoutConstraint activateConstraints:self.compactSizeClassConstraints];
    fontStyle = UIFontTextStyleSubheadline;
  }
  [self applyDefaultSizeWithButton:self.primaryActionButton
                         fontStyle:fontStyle];
  [self applyDefaultSizeWithButton:self.secondaryActionButton
                         fontStyle:fontStyle];

  // For larger texts update the layout to display buttons centered on the
  // vertical axis.
  if (UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory)) {
    self.actionButtonsView.axis = UILayoutConstraintAxisVertical;
  } else {
    self.actionButtonsView.axis = UILayoutConstraintAxisHorizontal;
  }
}

#pragma mark - Properties

- (UIColor*)systemBackgroundColor {
  return [UIColor colorNamed:kPrimaryBackgroundColor];
}

- (NSString*)secondaryActionButtonTitle {
  return l10n_util::GetNSString(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
}

- (int)acceptSigninButtonStringId {
  return IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON;
}

- (const AuthenticationViewConstants&)authenticationViewConstants {
  BOOL isRegularSizeClass = IsRegularXRegularSizeClass(self.traitCollection);
  return isRegularSizeClass ? kRegularConstants : kCompactConstants;
}

- (UIView*)gradientView {
  if (!_gradientView) {
    _gradientView = [[GradientView alloc]
        initWithTopColor:[[UIColor colorNamed:kPrimaryBackgroundColor]
                             colorWithAlphaComponent:0]
             bottomColor:[UIColor colorNamed:kPrimaryBackgroundColor]];
  }
  return _gradientView;
}

- (NSArray*)compactSizeClassConstraints {
  if (!_compactSizeClassConstraints) {
    NSMutableArray* constraints =
        [self generateConstraintsWithConstants:kCompactConstants];
    [constraints addObjectsFromArray:@[
      // Constraints for the user consent view inside the container view.
      [self.embeddedViewController.view.topAnchor
          constraintEqualToAnchor:self.containerView.topAnchor],
      [self.embeddedViewController.view.bottomAnchor
          constraintEqualToAnchor:self.containerView.bottomAnchor],
      [self.embeddedViewController.view.leadingAnchor
          constraintEqualToAnchor:self.containerView.leadingAnchor],
      [self.embeddedViewController.view.trailingAnchor
          constraintEqualToAnchor:self.containerView.trailingAnchor],
      // Constraint between the container view and the horizontal buttons.
      [self.actionButtonsView.topAnchor
          constraintEqualToAnchor:self.containerView.bottomAnchor
                         constant:kCompactConstants.ButtonVerticalPadding],
    ]];
    _compactSizeClassConstraints = constraints;
  }
  return _compactSizeClassConstraints;
}

- (NSArray*)regularSizeClassConstraints {
  if (!_regularSizeClassConstraints) {
    NSMutableArray* constraints =
        [self generateConstraintsWithConstants:kRegularConstants];
    [constraints addObjectsFromArray:@[
      // Constraints for the user consent view inside the container view, to
      // make sure it is never bigger than the container view.
      [self.embeddedViewController.view.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.containerView.topAnchor],
      [self.embeddedViewController.view.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.containerView.bottomAnchor],
      [self.embeddedViewController.view.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:self.containerView
                                                   .leadingAnchor],
      [self.embeddedViewController.view.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.containerView.trailingAnchor],
      // The user consent view needs to be centered if the container view is
      // bigger than the max size authorized for the user consent view.
      [self.embeddedViewController.view.centerXAnchor
          constraintEqualToAnchor:self.containerView.centerXAnchor],
      [self.embeddedViewController.view.centerYAnchor
          constraintEqualToAnchor:self.containerView.centerYAnchor],
      // Constraint between the container view and the horizontal buttons.
      [self.actionButtonsView.topAnchor
          constraintEqualToAnchor:self.containerView.bottomAnchor
                         constant:kRegularConstants.ButtonVerticalPadding],
    ]];
    // Adding constraints to ensure the user consent view has a limited size
    // on iPad. If the screen is bigger than the max size, those constraints
    // limit the user consent view.
    // If the screen is smaller than the max size, those constraints are ignored
    // since they have a lower priority than the constraints set aboved.
    NSArray* lowerPriorityConstraints = @[
      [self.embeddedViewController.view.heightAnchor
          constraintEqualToConstant:kUserConsentMaxSize],
      [self.embeddedViewController.view.widthAnchor
          constraintEqualToConstant:kUserConsentMaxSize],
    ];
    for (NSLayoutConstraint* layout_constraints in lowerPriorityConstraints) {
      // We do not use `UILayoutPriorityDefaultHigh` because it makes some
      // multiline labels on one line and truncated on iPad.
      layout_constraints.priority = UILayoutPriorityRequired - 1;
    }
    [constraints addObjectsFromArray:lowerPriorityConstraints];
    _regularSizeClassConstraints = constraints;
  }
  return _regularSizeClassConstraints;
}

#pragma mark - Subviews

// Sets up activity indicator properties and adds it to the user sign-in view.
- (void)addActivityIndicator {
  DCHECK(!self.activityIndicator);
  self.activityIndicator =
      [[MDCActivityIndicator alloc] initWithFrame:CGRectZero];
  self.activityIndicator.strokeWidth = 3;
  self.activityIndicator.cycleColors = @[ [UIColor colorNamed:kBlueColor] ];

  [self.view addSubview:self.activityIndicator];

  self.activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameCenterConstraints(self.containerView, self.activityIndicator);
}

// Sets the text, styling, and other button properties for the skip sign-in
// button.
- (void)updateSecondaryButtonStyle {
  DCHECK(self.secondaryActionButton);
  self.secondaryActionButton.accessibilityIdentifier =
      kSkipSigninAccessibilityIdentifier;
  [self.secondaryActionButton setTitle:self.secondaryActionButtonTitle
                              forState:UIControlStateNormal];

  [self updateButtonStyleWithButton:self.secondaryActionButton
                              style:SECONDARY_ACTION_STYLE];
}

#pragma mark - Styling

- (void)updateButtonStyleWithButton:(UIButton*)button
                              style:(ActionButtonStyle)style {
  switch (style) {
    case PRIMARY_ACTION_STYLE: {
      // Set the blue background button styling.
      button.backgroundColor = [UIColor colorNamed:kBlueColor];
      button.layer.cornerRadius = kButtonCornerRadius;

      UIButtonConfiguration* buttonConfiguration = button.configuration;
      buttonConfiguration.baseForegroundColor =
          [UIColor colorNamed:kSolidButtonTextColor];
      button.configuration = buttonConfiguration;
      break;
    }
    case SECONDARY_ACTION_STYLE: {
      // Set the blue text button styling.
      UIButtonConfiguration* buttonConfiguration = button.configuration;
      buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlueColor];
      button.configuration = buttonConfiguration;
      break;
    }
  }

  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider =
      CreateOpaqueOrTransparentButtonPointerStyleProvider();
}

// Applies font and inset to `button` according to the current size class.
- (void)applyDefaultSizeWithButton:(UIButton*)button
                         fontStyle:(UIFontTextStyle)fontStyle {
  const AuthenticationViewConstants& constants =
      self.authenticationViewConstants;
  CGFloat horizontalContentInset = constants.ButtonTitleContentHorizontalInset;
  CGFloat verticalContentInset = constants.ButtonTitleContentVerticalInset;

  UIButtonConfiguration* buttonConfiguration = button.configuration;
  buttonConfiguration.contentInsets =
      NSDirectionalEdgeInsetsMake(verticalContentInset, horizontalContentInset,
                                  verticalContentInset, horizontalContentInset);
  NSString* title = button.configuration.title;
  if (title) {
    UIFont* font = [UIFont preferredFontForTextStyle:fontStyle];
    NSDictionary* attributes = @{NSFontAttributeName : font};
    NSAttributedString* attributedTitle =
        [[NSAttributedString alloc] initWithString:title attributes:attributes];
    buttonConfiguration.attributedTitle = attributedTitle;
  }
  button.configuration = buttonConfiguration;
}

#pragma mark - Events

- (void)onSecondaryActionButtonPressed:(id)sender {
  DCHECK_EQ(self.secondaryActionButton, sender);
  [self.delegate userSigninViewControllerDidTapOnSkipSignin];
}

- (void)onPrimaryActionButtonPressed:(id)sender {
  DCHECK_EQ(self.primaryActionButton, sender);

  switch (self.primaryActionButton.tag) {
    case AuthenticationButtonTypeMore: {
      [self.delegate userSigninViewControllerDidScrollOnUnifiedConsent];
      break;
    }
    case AuthenticationButtonTypeAddAccount: {
      [self.delegate userSigninViewControllerDidTapOnAddAccount];
      break;
    }
    case AuthenticationButtonTypeConfirmation: {
      [self.delegate userSigninViewControllerDidTapOnSignin];
      break;
    }
  }
}

@end
