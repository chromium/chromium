// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_view_controller.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "components/sync/base/user_selectable_type.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_layout_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/browser/ui/authentication/views/identity_view.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Margins for `self.contentView` (top, bottom, leading and trailing).
constexpr CGFloat kContentMargin = 16.;
// Space between elements in `self.contentView`.
constexpr CGFloat kContentSpacing = 16.;
// Extra top padding between the navigation bar of the scroll view and the
// top edge of the scroll view.
constexpr CGFloat kExtraNavBarTopPadding = 3.;
// Vertical insets of primary button.
constexpr CGFloat kPrimaryButtonVerticalInsets = 15.5;

// The label the bottom sheet should display, or null if there should be none.
// The label should never promise "sign in to achieve X" if an enterprise
// policy is preventing X.
NSString* GetPromoLabelString(
    signin_metrics::AccessPoint access_point,
    bool sync_transport_disabled_by_policy,
    syncer::UserSelectableTypeSet sync_types_disabled_by_policy) {
  // TODO(crbug.com/1468530): Convert DUMP_WILL_BE_CHECKs to CHECKs (some are
  // probably failing now).
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO:
      // Sign-in shouldn't be offered if the feature doesn't work.
      DUMP_WILL_BE_CHECK(!sync_transport_disabled_by_policy);
      return l10n_util::GetNSString(IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_LABEL);
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO:
      // Configuring feed interests is independent of sync.
      return l10n_util::GetNSString(IDS_IOS_FEED_CARD_SIGN_IN_ONLY_PROMO_LABEL);
    case signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN:
      if (!base::FeatureList::IsEnabled(
              syncer::kReplaceSyncPromosWithSignInPromos)) {
        return l10n_util::GetNSString(
            IDS_IOS_CONSISTENCY_PROMO_DEFAULT_ACCOUNT_LABEL);
      }
      // This could check `sync_types_disabled_by_policy` only for the types
      // mentioned in the regular string, but don't bother.
      return sync_transport_disabled_by_policy ||
                     !sync_types_disabled_by_policy.Empty()
                 ? l10n_util::GetNSString(
                       IDS_IOS_CONSISTENCY_PROMO_DEFAULT_ACCOUNT_LABEL)
                 : l10n_util::GetNSString(
                       IDS_IOS_SIGNIN_SHEET_LABEL_FOR_WEB_SIGNIN);
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_SIGNED_OUT_ICON:
      // This could check `sync_types_disabled_by_policy` only for the types
      // mentioned in the regular string, but don't bother.
      return sync_transport_disabled_by_policy ||
                     !sync_types_disabled_by_policy.Empty()
                 ? nil
                 : l10n_util::GetNSString(
                       IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL);
    case signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST:
      // "Sync" is mentioned in the setup list (the card, not this sheet). So it
      // was easier to hide it than come up with new strings. In the future, we
      // could tweak the card strings and return nil here.
      DUMP_WILL_BE_CHECK(!sync_transport_disabled_by_policy &&
                         sync_types_disabled_by_policy.Empty());
      return l10n_util::GetNSString(IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL);
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO:
      // Feed personalization is independent of sync.
      return l10n_util::GetNSString(IDS_IOS_SIGNIN_SHEET_LABEL_FOR_FEED_PROMO);
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
      // Sign-in shouldn't be offered if the feature doesn't work.
      DUMP_WILL_BE_CHECK(!sync_transport_disabled_by_policy &&
                         !sync_types_disabled_by_policy.Has(
                             syncer::UserSelectableType::kTabs));
      return l10n_util::GetNSString(IDS_IOS_SIGNIN_SHEET_LABEL_FOR_RECENT_TABS);
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      // No text.
      return nil;
    case signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_LINK:
    case signin_metrics::AccessPoint::ACCESS_POINT_MENU:
    case signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
    case signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_DEVICES_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CLOUD_PRINT:
    case signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
    case signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON:
    case signin_metrics::AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_SYNC_ERROR_CARD:
    case signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_RENAMED:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAFETY_CHECK:
    case signin_metrics::AccessPoint::ACCESS_POINT_KALEIDOSCOPE:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS_SYNC_OFF_ROW:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_BACKGROUND_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CREATOR_FEED_FOLLOW:
    case signin_metrics::AccessPoint::ACCESS_POINT_READING_LIST:
    case signin_metrics::AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
      // Nothing prevents instantiating ConsistencyDefaultAccountViewController
      // with an arbitrary entry point, API-wise. In doubt, no label is a good,
      // generic default that fits all entry points.
      return nil;
    case signin_metrics::AccessPoint::ACCESS_POINT_MAX:
      NOTREACHED_NORETURN();
  }
}

}  // namespace

@interface ConsistencyDefaultAccountViewController ()

// View that contains all UI elements for the view controller. This view is
// the only subview of -[ConsistencyDefaultAccountViewController view].
@property(nonatomic, strong) UIStackView* contentView;
// Button to present the default identity.
@property(nonatomic, strong) IdentityButtonControl* identityButtonControl;
// Button to
// 1. confirm the default identity and sign-in when an account is available, or
// 2. add an account when no account is available on the device.
@property(nonatomic, strong) UIButton* primaryButton;
// Title for `self.primaryButton` when it needs to show the text "Continue as…".
// This property is needed to hide the title the activity indicator is shown.
@property(nonatomic, strong) NSString* continueAsTitle;
// Activity indicator on top of `self.primaryButton`.
@property(nonatomic, strong) UIActivityIndicatorView* activityIndicatorView;
// The access point that triggered sign-in.
@property(nonatomic, assign, readonly) signin_metrics::AccessPoint accessPoint;
// Whether the sync transport layer got disabled by an enterprise policy. See
// also the comment in the corresponding ConsistencyDefaultAccountConsumer
// setter as to how this might differ from `syncTypesDisabledByPolicy` below.
@property(nonatomic, assign, readwrite) BOOL syncTransportDisabledByPolicy;
// Whether individual sync types got disabled by an enterprise policy.
@property(nonatomic, assign, readwrite)
    syncer::UserSelectableTypeSet syncTypesDisabledByPolicy;

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
  self.activityIndicatorView.color = [UIColor colorNamed:kSolidButtonTextColor];
  [self.primaryButton addSubview:self.activityIndicatorView];
  AddSameCenterConstraints(self.activityIndicatorView, self.primaryButton);
  [self.activityIndicatorView startAnimating];
  // Disable buttons.
  self.identityButtonControl.enabled = NO;
  self.primaryButton.enabled = NO;
  [self.primaryButton setTitle:@"" forState:UIControlStateNormal];
}

- (void)stopSpinner {
  // Remove spinner.
  DCHECK(self.activityIndicatorView);
  [self.activityIndicatorView removeFromSuperview];
  self.activityIndicatorView = nil;
  // Show the IdentityButtonControl, since it may be hidden.
  self.identityButtonControl.hidden = NO;
  // Enable buttons.
  self.identityButtonControl.enabled = YES;
  self.primaryButton.enabled = YES;
  DCHECK(self.continueAsTitle);
  [self.primaryButton setTitle:self.continueAsTitle
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

  NSString* skipButtonTitle =
      self.accessPoint == signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN
          ? l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_SKIP)
          : l10n_util::GetNSString(IDS_CANCEL);
  UIButton* skipButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [skipButton setTitle:skipButtonTitle forState:UIControlStateNormal];
  skipButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  [skipButton addTarget:self
                 action:@selector(skipButtonAction:)
       forControlEvents:UIControlEventTouchUpInside];
  skipButton.accessibilityIdentifier =
      kWebSigninSkipButtonAccessibilityIdentifier;
  // Put titleLabel and skipButton each in a wrapper UIView so we can adjust
  // their top padding.
  UIView* titleLabelWrapper = [[UIView alloc] init];
  UIView* skipButtonWrapper = [[UIView alloc] init];
  [titleLabelWrapper addSubview:titleLabel];
  [skipButtonWrapper addSubview:skipButton];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  skipButton.translatesAutoresizingMaskIntoConstraints = NO;

  // Fix the positions of titleLabel and skipButton relative to their wrappers.
  [NSLayoutConstraint activateConstraints:@[
    [titleLabel.topAnchor constraintEqualToAnchor:titleLabelWrapper.topAnchor
                                         constant:kExtraNavBarTopPadding],
    [titleLabel.bottomAnchor
        constraintEqualToAnchor:titleLabelWrapper.bottomAnchor],
    [titleLabel.leadingAnchor
        constraintEqualToAnchor:titleLabelWrapper.leadingAnchor],
    [titleLabel.trailingAnchor
        constraintEqualToAnchor:titleLabelWrapper.trailingAnchor],
    [skipButton.topAnchor constraintEqualToAnchor:skipButtonWrapper.topAnchor
                                         constant:kExtraNavBarTopPadding],
    [skipButton.bottomAnchor
        constraintEqualToAnchor:skipButtonWrapper.bottomAnchor],
    [skipButton.leadingAnchor
        constraintEqualToAnchor:skipButtonWrapper.leadingAnchor],
    [skipButton.trailingAnchor
        constraintEqualToAnchor:skipButtonWrapper.trailingAnchor]
  ]];

  // Add the wrappers to the navigation bar.
  UIBarButtonItem* leftItem =
      [[UIBarButtonItem alloc] initWithCustomView:titleLabelWrapper];
  UIBarButtonItem* rightItem =
      [[UIBarButtonItem alloc] initWithCustomView:skipButtonWrapper];
  self.navigationItem.leftBarButtonItem = leftItem;
  self.navigationItem.rightBarButtonItem = rightItem;

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
  NSString* labelText =
      GetPromoLabelString(self.accessPoint, self.syncTransportDisabledByPolicy,
                          self.syncTypesDisabledByPolicy);
  if (labelText) {
    UILabel* label = [[UILabel alloc] init];
    label.text = labelText;
    label.textColor = [UIColor colorNamed:kGrey700Color];
    label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    label.numberOfLines = 0;
    [self.contentView addArrangedSubview:label];
    [label.widthAnchor constraintEqualToAnchor:self.contentView.widthAnchor]
        .active = YES;
  }

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
  // Add the primary button (the "Continue as"/"Sign in" button).
  self.primaryButton =
      PrimaryActionButton(/* pointer_interaction_enabled */ YES);
  SetContentEdgeInsets(self.primaryButton,
                       UIEdgeInsetsMake(kPrimaryButtonVerticalInsets, 0,
                                        kPrimaryButtonVerticalInsets, 0));
  self.primaryButton.accessibilityIdentifier =
      kWebSigninPrimaryButtonAccessibilityIdentifier;
  self.primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.primaryButton addTarget:self
                         action:@selector(primaryButtonAction:)
               forControlEvents:UIControlEventTouchUpInside];

  [self.contentView addArrangedSubview:self.primaryButton];

  [NSLayoutConstraint activateConstraints:@[
    [self.primaryButton.widthAnchor
        constraintEqualToAnchor:self.contentView.widthAnchor]
  ]];
  // Adjust the identity button control rounded corners to the same value than
  // the "continue as" button.
  self.identityButtonControl.layer.cornerRadius =
      self.primaryButton.layer.cornerRadius;

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

- (void)primaryButtonAction:
    (ConsistencyDefaultAccountViewController*)viewController {
  // If the IdentityButtonControl is hidden, there is no account avaiable on the
  // device.
  if (!self.identityButtonControl.hidden) {
    [self.actionDelegate
        consistencyDefaultAccountViewControllerContinueWithSelectedIdentity:
            self];
  } else {
    [self.actionDelegate
        consistencyDefaultAccountViewControllerAddAccountAndSignin:self];
  }
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

- (void)showDefaultAccountWithFullName:(NSString*)fullName
                             givenName:(NSString*)givenName
                                 email:(NSString*)email
                                avatar:(UIImage*)avatar {
  if (!self.viewLoaded) {
    // Load the view.
    [self view];
  }
  self.continueAsTitle = l10n_util::GetNSStringF(
      IDS_IOS_SIGNIN_PROMO_CONTINUE_AS, base::SysNSStringToUTF16(givenName));

  [self.identityButtonControl setIdentityName:fullName email:email];
  [self.identityButtonControl setIdentityAvatar:avatar];

  // If spinner is active, delay UI updates until stopSpinner() is called.
  if (!self.activityIndicatorView) {
    [self.primaryButton setTitle:self.continueAsTitle
                        forState:UIControlStateNormal];
    self.identityButtonControl.hidden = NO;
  }
}

- (void)hideDefaultAccount {
  if (!self.viewLoaded) {
    [self view];
  }

  // Hide the IdentityButtonControl, and update the primary button to serve as
  // a "Sign in…" button.
  self.identityButtonControl.hidden = YES;
  [self.primaryButton
      setTitle:l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_SIGN_IN)
      forState:UIControlStateNormal];
}

@end
