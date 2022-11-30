// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_layout_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/browser/ui/authentication/views/identity_view.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Margins for `self.contentView` (top, bottom, leading and trailing).
constexpr CGFloat kContentMargin = 16.;
// Space between elements in `self.contentView`.
constexpr CGFloat kContentSpacing = 16.;

}

@interface ConsistencyDefaultAccountViewController ()

// View that contains all UI elements for the view controller. This view is
// the only subview of -[ConsistencyDefaultAccountViewController view].
@property(nonatomic, strong) UIStackView* contentView;
// Button to present the default identity.
@property(nonatomic, strong) IdentityButtonControl* identityButtonControl;
// Button to confirm the default identity and sign-in.
@property(nonatomic, strong) UIButton* continueAsButton;
// Title for `self.continueAsButton`. This property is needed to hide the title
// the activity indicator is shown.
@property(nonatomic, strong) NSString* continueAsTitle;
// Activity indicator on top of `self.continueAsButton`.
@property(nonatomic, strong) UIActivityIndicatorView* activityIndicatorView;
// The access point that triggered sign-in.
@property(nonatomic, assign, readonly) signin_metrics::AccessPoint accessPoint;

@end

@implementation ConsistencyDefaultAccountViewController

- (instancetype)initWithAccessPoint:(signin_metrics::AccessPoint)accessPoint {
  self = [super init];
  if (self) {
    _accessPoint = accessPoint;
  }
  return self;
}

- (void)startSpinner {
  // Add spinner.
  DCHECK(!self.activityIndicatorView);
  self.activityIndicatorView = [[UIActivityIndicatorView alloc] init];
  self.activityIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
  self.activityIndicatorView.color = UIColor.whiteColor;
  [self.continueAsButton addSubview:self.activityIndicatorView];
  AddSameCenterConstraints(self.activityIndicatorView, self.continueAsButton);
  [self.activityIndicatorView startAnimating];
  // Disable buttons.
  self.identityButtonControl.enabled = NO;
  self.continueAsButton.enabled = NO;
  [self.continueAsButton setTitle:@"" forState:UIControlStateNormal];
}

- (void)stopSpinner {
  // Remove spinner.
  DCHECK(self.activityIndicatorView);
  [self.activityIndicatorView removeFromSuperview];
  self.activityIndicatorView = nil;
  // Enable buttons.
  self.identityButtonControl.enabled = YES;
  self.continueAsButton.enabled = YES;
  DCHECK(self.continueAsTitle);
  [self.continueAsButton setTitle:self.continueAsTitle
                         forState:UIControlStateNormal];
}

#pragma mark - UIViewController

- (void)viewWillAppear:(BOOL)animated {
  self.identityButtonControl.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  // Set the navigation title in the left bar button item to have left
  // alignment.
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_DEFAULT_ACCOUNT_TITLE);
  titleLabel.textAlignment = NSTextAlignmentLeft;
  titleLabel.adjustsFontSizeToFitWidth = YES;
  titleLabel.minimumScaleFactor = 0.1;
  UIBarButtonItem* leftItem =
      [[UIBarButtonItem alloc] initWithCustomView:titleLabel];
  self.navigationItem.leftBarButtonItem = leftItem;

  // Set the skip button in the right bar button item.
  NSString* skipButtonTitle =
      self.accessPoint ==
              signin_metrics::AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO
          ? l10n_util::GetNSString(IDS_CANCEL)
          : l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_SKIP);
  UIBarButtonItem* skipButton =
      [[UIBarButtonItem alloc] initWithTitle:skipButtonTitle
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(skipButtonAction:)];
  skipButton.accessibilityIdentifier =
      kWebSigninSkipButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = skipButton;
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
  self.contentView = [[UIStackView alloc] init];
  self.contentView.axis = UILayoutConstraintAxisVertical;
  self.contentView.distribution = UIStackViewDistributionEqualSpacing;
  self.contentView.alignment = UIStackViewAlignmentCenter;
  self.contentView.spacing = kContentSpacing;
  self.contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollView addSubview:self.contentView];
  UILayoutGuide* contentLayoutGuide = scrollView.contentLayoutGuide;
  UILayoutGuide* frameLayoutGuide = scrollView.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [contentLayoutGuide.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor
                       constant:-kContentMargin],
    [contentLayoutGuide.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor
                       constant:kContentMargin],
    [frameLayoutGuide.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:-kContentMargin],
    [frameLayoutGuide.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:kContentMargin],
  ]];
  // Add the label.
  UILabel* label = [[UILabel alloc] init];
  if (self.accessPoint ==
      signin_metrics::AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO) {
    label.text =
        l10n_util::GetNSString(IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_LABEL);
  } else {
    label.text =
        l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_DEFAULT_ACCOUNT_LABEL);
  }

  label.textColor = [UIColor colorNamed:kGrey700Color];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  label.numberOfLines = 0;
  [self.contentView addArrangedSubview:label];
  [label.widthAnchor constraintEqualToAnchor:self.contentView.widthAnchor]
      .active = YES;
  // Add IdentityButtonControl for the default identity.
  self.identityButtonControl =
      [[IdentityButtonControl alloc] initWithFrame:CGRectZero];
  self.identityButtonControl.arrowDirection = IdentityButtonControlArrowRight;
  self.identityButtonControl.identityViewStyle = IdentityViewStyleConsistency;
  [self.identityButtonControl addTarget:self
                                 action:@selector(identityButtonControlAction:
                                                                     forEvent:)
                       forControlEvents:UIControlEventTouchUpInside];
  [self.contentView addArrangedSubview:self.identityButtonControl];
  [NSLayoutConstraint activateConstraints:@[
    [self.identityButtonControl.widthAnchor
        constraintEqualToAnchor:self.contentView.widthAnchor]
  ]];
  // Add primary button.
  self.continueAsButton =
      PrimaryActionButton(/* pointer_interaction_enabled */ YES);
  self.continueAsButton.accessibilityIdentifier =
      kWebSigninContinueAsButtonAccessibilityIdentifier;
  self.continueAsButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.continueAsButton addTarget:self
                            action:@selector(signInWithDefaultIdentityAction:)
                  forControlEvents:UIControlEventTouchUpInside];
  [self.contentView addArrangedSubview:self.continueAsButton];
  [NSLayoutConstraint activateConstraints:@[
    [self.continueAsButton.widthAnchor
        constraintEqualToAnchor:self.contentView.widthAnchor]
  ]];
  // Adjust the identity button control rounded corners to the same value than
  // the "continue as" button.
  self.identityButtonControl.layer.cornerRadius =
      self.continueAsButton.layer.cornerRadius;

  // Ensure that keyboard is hidden.
  UIResponder* firstResponder = GetFirstResponder();
  [firstResponder resignFirstResponder];
}

#pragma mark - UI actions

- (void)skipButtonAction:(id)sender {
  [self.actionDelegate consistencyDefaultAccountViewControllerSkip:self];
}

- (void)identityButtonControlAction:(id)sender forEvent:(UIEvent*)event {
  [self.actionDelegate
      consistencyDefaultAccountViewControllerOpenIdentityChooser:self];
}

- (void)signInWithDefaultIdentityAction:(id)sender {
  [self.actionDelegate
      consistencyDefaultAccountViewControllerContinueWithSelectedIdentity:self];
}

#pragma mark - ChildConsistencySheetViewController

- (CGFloat)layoutFittingHeightForWidth:(CGFloat)width {
  CGFloat contentViewWidth = width - self.view.safeAreaInsets.left -
                             self.view.safeAreaInsets.right -
                             kContentMargin * 2;
  CGSize size = CGSizeMake(contentViewWidth, 0);
  size = [self.contentView
        systemLayoutSizeFittingSize:size
      withHorizontalFittingPriority:UILayoutPriorityRequired
            verticalFittingPriority:UILayoutPriorityFittingSizeLevel];
  CGFloat safeAreaInsetsHeight = 0;
  switch (self.layoutDelegate.displayStyle) {
    case ConsistencySheetDisplayStyleBottom:
      safeAreaInsetsHeight =
          self.navigationController.view.window.safeAreaInsets.bottom;
      break;
    case ConsistencySheetDisplayStyleCentered:
      break;
  }
  // Safe area insets needs to be based on the window since the `self.view`
  // might not be part of the window hierarchy when the animation is configured.
  return self.navigationController.navigationBar.frame.size.height +
         kContentMargin + size.height + kContentMargin + safeAreaInsetsHeight;
}

#pragma mark - ConsistencyDefaultAccountConsumer

- (void)updateWithFullName:(NSString*)fullName
                 givenName:(NSString*)givenName
                     email:(NSString*)email {
  if (!self.viewLoaded) {
    // Load the view.
    [self view];
  }
  self.continueAsTitle = l10n_util::GetNSStringF(
      IDS_IOS_SIGNIN_PROMO_CONTINUE_AS, base::SysNSStringToUTF16(givenName));
  [self.continueAsButton setTitle:self.continueAsTitle
                         forState:UIControlStateNormal];
  [self.identityButtonControl setIdentityName:fullName email:email];
}

- (void)updateUserAvatar:(UIImage*)avatar {
  if (!self.viewLoaded) {
    // Load the view.
    [self view];
  }
  [self.identityButtonControl setIdentityAvatar:avatar];
}

@end
