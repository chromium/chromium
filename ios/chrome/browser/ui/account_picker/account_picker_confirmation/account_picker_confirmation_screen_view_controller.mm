// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/account_picker/account_picker_confirmation/account_picker_confirmation_screen_view_controller.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_configuration.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_confirmation/account_picker_confirmation_screen_constants.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_layout_delegate.h"
#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/browser/ui/authentication/views/identity_view.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Margins for `_contentView` (top, bottom, leading and trailing).
constexpr CGFloat kContentMargin = 16.;
// Space between elements in `_contentView`.
constexpr CGFloat kContentSpacing = 16.;
// Vertical insets of primary button.
constexpr CGFloat kPrimaryButtonVerticalInsets = 15.5;

// Returns font to use for the navigation bar title.
UIFont* GetNavigationBarTitleFont() {
  UITraitCollection* large_trait_collection =
      [UITraitCollection traitCollectionWithPreferredContentSizeCategory:
                             UIContentSizeCategoryLarge];
  UIFontDescriptor* font_descriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleBody
             compatibleWithTraitCollection:large_trait_collection];
  UIFont* font = [UIFont systemFontOfSize:font_descriptor.pointSize
                                   weight:UIFontWeightBold];
  return [[UIFontMetrics defaultMetrics] scaledFontForFont:font];
}

// Returns the width and height of a single pixel in point.
CGFloat GetPixelLength() {
  return 1.0 / [UIScreen mainScreen].scale;
}

}  // namespace

@implementation AccountPickerConfirmationScreenViewController {
  // View that contains all UI elements for the view controller. This view is
  // the only subview of -[AccountPickerConfirmationScreenViewController view].
  __strong UIStackView* _contentView;
  // Button to present the default identity.
  __strong IdentityButtonControl* _identityButtonControl;
  // "Grouped" section containing the identity button and the switch.
  // If there is no switch, then this is equal to `identityButtonControl`.
  __strong UIView* _groupedIdentityButtonSection;
  // Button to
  // 1. confirm the default identity and sign-in when an account is available,
  // or
  // 2. add an account when no account is available on the device.
  __strong UIButton* _primaryButton;
  // Title for `_primaryButton` when it needs to show the text "Continue
  // as…". This property is needed to hide the title the activity indicator is
  // shown.
  __strong NSString* _submitString;
  // Activity indicator on top of `_primaryButton`.
  __strong UIActivityIndicatorView* _activityIndicatorView;
  // Switch to let the user decide whether they want to choose an account every
  // time. Only shown if `_showAskEveryTimeSwitch` is YES.
  __strong UISwitch* _askEveryTimeSwitch;
  // The account picker configuration.
  __strong AccountPickerConfiguration* _configuration;
}

- (instancetype)initWithConfiguration:
    (AccountPickerConfiguration*)configuration {
  self = [super init];
  if (self) {
    _configuration = configuration;
  }
  return self;
}

- (void)startSpinner {
  // Add spinner.
  DCHECK(!_activityIndicatorView);
  _activityIndicatorView = [[UIActivityIndicatorView alloc] init];
  _activityIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
  _activityIndicatorView.color = [UIColor colorNamed:kSolidButtonTextColor];
  [_primaryButton addSubview:_activityIndicatorView];
  AddSameCenterConstraints(_activityIndicatorView, _primaryButton);
  [_activityIndicatorView startAnimating];
  // Disable buttons.
  _identityButtonControl.enabled = NO;
  _askEveryTimeSwitch.enabled = NO;
  _primaryButton.enabled = NO;
  SetConfigurationTitle(_primaryButton, @" ");
}

- (void)stopSpinner {
  // Remove spinner.
  DCHECK(_activityIndicatorView);
  [_activityIndicatorView removeFromSuperview];
  _activityIndicatorView = nil;
  // Show the IdentityButtonControl, since it may be hidden.
  _identityButtonControl.hidden = NO;
  // Enable buttons.
  _identityButtonControl.enabled = YES;
  _askEveryTimeSwitch.enabled = YES;
  _primaryButton.enabled = YES;
  DCHECK(_submitString);
  SetConfigurationTitle(_primaryButton, _submitString);
}

#pragma mark - UIViewController

- (void)viewWillAppear:(BOOL)animated {
  _identityButtonControl.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  // Set the navigation title in the left bar button item to have left
  // alignment.
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.font = GetNavigationBarTitleFont();
  titleLabel.text = _configuration.titleText;
  titleLabel.textAlignment = NSTextAlignmentLeft;
  titleLabel.adjustsFontSizeToFitWidth = YES;
  titleLabel.minimumScaleFactor = 0.1;

  // Add the title label to the navigation bar.
  UIBarButtonItem* leftItem =
      [[UIBarButtonItem alloc] initWithCustomView:titleLabel];
  self.navigationItem.leftBarButtonItem = leftItem;
  self.navigationController.navigationBar.minimumContentSizeCategory =
      UIContentSizeCategoryLarge;
  self.navigationController.navigationBar.maximumContentSizeCategory =
      UIContentSizeCategoryExtraExtraLarge;
  // Create the skip button.
  UIBarButtonItem* cancelButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonAction:)];
  cancelButtonItem.accessibilityIdentifier =
      kAccountPickerCancelButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = cancelButtonItem;

  // Replace the controller view by the scroll view.
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:scrollView];
  [NSLayoutConstraint activateConstraints:@[
    [scrollView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  // Create content view.
  _contentView = [[UIStackView alloc] init];
  _contentView.axis = UILayoutConstraintAxisVertical;
  _contentView.distribution = UIStackViewDistributionEqualSpacing;
  _contentView.alignment = UIStackViewAlignmentCenter;
  _contentView.spacing = kContentSpacing;
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollView addSubview:_contentView];
  UILayoutGuide* contentLayoutGuide = scrollView.contentLayoutGuide;
  UILayoutGuide* frameLayoutGuide = scrollView.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [contentLayoutGuide.topAnchor constraintEqualToAnchor:_contentView.topAnchor
                                                 constant:-kContentMargin],
    [contentLayoutGuide.bottomAnchor
        constraintEqualToAnchor:_contentView.bottomAnchor
                       constant:kContentMargin],
    [frameLayoutGuide.leadingAnchor
        constraintEqualToAnchor:_contentView.leadingAnchor
                       constant:-kContentMargin],
    [frameLayoutGuide.trailingAnchor
        constraintEqualToAnchor:_contentView.trailingAnchor
                       constant:kContentMargin],
  ]];

  // Add the label.
  NSString* bodyText = _configuration.bodyText;
  if (bodyText) {
    UILabel* label = [[UILabel alloc] init];
    label.adjustsFontForContentSizeCategory = YES;
    label.text = bodyText;
    label.textColor = [UIColor colorNamed:kGrey700Color];
    label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    label.numberOfLines = 0;
    [_contentView addArrangedSubview:label];
    [label.widthAnchor constraintEqualToAnchor:_contentView.widthAnchor]
        .active = YES;
  }

  // Add IdentityButtonControl for the default identity.
  _identityButtonControl =
      [[IdentityButtonControl alloc] initWithFrame:CGRectZero];
  _identityButtonControl.arrowDirection = IdentityButtonControlArrowRight;
  _identityButtonControl.identityViewStyle = IdentityViewStyleConsistency;
  [_identityButtonControl addTarget:self
                             action:@selector(identityButtonControlAction:
                                                                 forEvent:)
                   forControlEvents:UIControlEventTouchUpInside];

  UIView* groupedIdentityButtonSection = _identityButtonControl;
  if (_configuration.askEveryTimeSwitchLabelText != nil) {
    _identityButtonControl.layer.cornerRadius = 0;
    UIStackView* identityStackView = [[UIStackView alloc] init];
    identityStackView.axis = UILayoutConstraintAxisVertical;
    identityStackView.translatesAutoresizingMaskIntoConstraints = NO;
    identityStackView.backgroundColor = _identityButtonControl.backgroundColor;
    identityStackView.clipsToBounds = YES;

    UIView* separator = [[UIView alloc] init];
    separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
    separator.translatesAutoresizingMaskIntoConstraints = NO;

    UIStackView* switchWithLabel = [[UIStackView alloc] init];
    switchWithLabel.axis = UILayoutConstraintAxisHorizontal;
    switchWithLabel.translatesAutoresizingMaskIntoConstraints = NO;
    switchWithLabel.alignment = UIStackViewAlignmentCenter;

    UILabel* askEveryTimeLabel = [[UILabel alloc] init];
    askEveryTimeLabel.text = _configuration.askEveryTimeSwitchLabelText;
    askEveryTimeLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    askEveryTimeLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    askEveryTimeLabel.numberOfLines = 0;
    askEveryTimeLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _askEveryTimeSwitch = [[UISwitch alloc] init];
    [_askEveryTimeSwitch addTarget:self
                            action:@selector(askEveryTimeSwitchAction:)
                  forControlEvents:UIControlEventValueChanged];
    _askEveryTimeSwitch.on = YES;
    [switchWithLabel addArrangedSubview:askEveryTimeLabel];
    [switchWithLabel addArrangedSubview:_askEveryTimeSwitch];

    UIView* switchWithLabelContainer = [[UIView alloc] init];
    switchWithLabelContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [switchWithLabelContainer addSubview:switchWithLabel];
    AddSameConstraintsWithInsets(switchWithLabel, switchWithLabelContainer,
                                 NSDirectionalEdgeInsetsMake(5., 16., 5., 16.));

    [identityStackView addArrangedSubview:_identityButtonControl];
    [identityStackView addArrangedSubview:separator];
    [identityStackView addArrangedSubview:switchWithLabelContainer];

    [NSLayoutConstraint activateConstraints:@[
      // Identity button constraints
      [_identityButtonControl.widthAnchor
          constraintEqualToAnchor:identityStackView.widthAnchor],
      // Separator constraints
      [separator.leadingAnchor
          constraintEqualToAnchor:identityStackView.leadingAnchor
                         constant:16.],
      [separator.trailingAnchor
          constraintEqualToAnchor:identityStackView.trailingAnchor],
      [separator.heightAnchor constraintEqualToConstant:GetPixelLength()],
      // Switch with label constraints
      [switchWithLabelContainer.widthAnchor
          constraintEqualToAnchor:identityStackView.widthAnchor],
      [switchWithLabelContainer.heightAnchor
          constraintGreaterThanOrEqualToConstant:58.]
    ]];

    groupedIdentityButtonSection = identityStackView;
  }

  _groupedIdentityButtonSection = groupedIdentityButtonSection;
  [_contentView addArrangedSubview:_groupedIdentityButtonSection];
  [NSLayoutConstraint activateConstraints:@[
    [_groupedIdentityButtonSection.widthAnchor
        constraintEqualToAnchor:_contentView.widthAnchor],
  ]];

  // Add the primary button (the "Continue as"/"Sign in" button).
  _primaryButton = PrimaryActionButton(/* pointer_interaction_enabled */ YES);
  UIButtonConfiguration* buttonConfiguration = _primaryButton.configuration;
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      kPrimaryButtonVerticalInsets, 0, kPrimaryButtonVerticalInsets, 0);
  _primaryButton.configuration = buttonConfiguration;

  _primaryButton.accessibilityIdentifier =
      kAccountPickerPrimaryButtonAccessibilityIdentifier;
  _primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_primaryButton addTarget:self
                     action:@selector(primaryButtonAction:)
           forControlEvents:UIControlEventTouchUpInside];

  [_contentView addArrangedSubview:_primaryButton];

  [NSLayoutConstraint activateConstraints:@[
    [_primaryButton.widthAnchor
        constraintEqualToAnchor:_contentView.widthAnchor]
  ]];
  // Adjust the identity button control rounded corners to the same value than
  // the "continue as" button.
  _groupedIdentityButtonSection.layer.cornerRadius =
      _primaryButton.configuration.background.cornerRadius;

  // Ensure that keyboard is hidden.
  UIResponder* firstResponder = GetFirstResponder();
  [firstResponder resignFirstResponder];
}

#pragma mark - UI actions

- (void)cancelButtonAction:(id)sender {
  [_actionDelegate accountPickerConfirmationScreenViewControllerCancel:self];
}

- (void)identityButtonControlAction:(id)sender forEvent:(UIEvent*)event {
  [_actionDelegate
      accountPickerConfirmationScreenViewControllerOpenAccountList:self];
}

- (void)askEveryTimeSwitchAction:(id)sender {
  [_actionDelegate
      accountPickerConfirmationScreenViewController:self
                                    setAskEveryTime:_askEveryTimeSwitch.on];
}

- (void)primaryButtonAction:
    (AccountPickerConfirmationScreenViewController*)viewController {
  [_actionDelegate
      accountPickerConfirmationScreenViewControllerContinueWithSelectedIdentity:
          self];
}

#pragma mark - AccountPickerScreenViewController

- (CGFloat)layoutFittingHeightForWidth:(CGFloat)width {
  CGFloat contentViewWidth = width - self.view.safeAreaInsets.left -
                             self.view.safeAreaInsets.right -
                             kContentMargin * 2;
  CGSize size = CGSizeMake(contentViewWidth, 0);
  size = [_contentView
        systemLayoutSizeFittingSize:size
      withHorizontalFittingPriority:UILayoutPriorityRequired
            verticalFittingPriority:UILayoutPriorityFittingSizeLevel];
  CGFloat safeAreaInsetsHeight = 0;
  switch (_layoutDelegate.displayStyle) {
    case AccountPickerSheetDisplayStyle::kBottom:
      safeAreaInsetsHeight =
          self.navigationController.view.window.safeAreaInsets.bottom;
      break;
    case AccountPickerSheetDisplayStyle::kCentered:
      break;
  }
  // Safe area insets needs to be based on the window since the `self.view`
  // might not be part of the window hierarchy when the animation is configured.
  return self.navigationController.navigationBar.frame.size.height +
         kContentMargin + size.height + kContentMargin + safeAreaInsetsHeight;
}

#pragma mark - AccountPickerConfirmationScreenConsumer

- (void)showDefaultAccountWithFullName:(NSString*)fullName
                             givenName:(NSString*)givenName
                                 email:(NSString*)email
                                avatar:(UIImage*)avatar {
  if (!self.viewLoaded) {
    // Load the view.
    [self view];
  }
  _submitString = _configuration.submitButtonTitle;

  [_identityButtonControl setIdentityName:fullName email:email];
  [_identityButtonControl setIdentityAvatar:avatar];

  // If spinner is active, delay UI updates until stopSpinner() is called.
  if (!_activityIndicatorView) {
    SetConfigurationTitle(_primaryButton, _submitString);
    _identityButtonControl.hidden = NO;
  }
}

- (void)hideDefaultAccount {
  if (!self.viewLoaded) {
    [self view];
  }

  // Hide the IdentityButtonControl, and update the primary button to serve as
  // a "Sign in…" button.
  _groupedIdentityButtonSection.hidden = YES;
  SetConfigurationTitle(_primaryButton, _configuration.submitButtonTitle);
}

@end
