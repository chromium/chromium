// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"

#include <stdint.h>
#include <cmath>
#include <memory>

#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>

#include "base/bind.h"
#include "base/feature_list.h"
#import "base/ios/block_types.h"
#include "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#include "ios/chrome/browser/signin/signin_util.h"
#include "ios/chrome/browser/sync/consent_auditor_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#include "ios/chrome/browser/ui/authentication/signin_account_selector_view_controller.h"
#include "ios/chrome/browser/ui/authentication/signin_confirmation_view_controller.h"
#include "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_coordinator.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Default animation duration.
const CGFloat kAnimationDuration = 0.5f;

// Minimum duration of the pending state in milliseconds.
const int64_t kMinimunPendingStateDurationMs = 300;

// Internal padding between the title and image in the "More" button.
const CGFloat kMoreButtonPadding = 5.0f;

struct AuthenticationViewConstants {
  CGFloat PrimaryFontSize;
  CGFloat SecondaryFontSize;
  CGFloat GradientHeight;
  CGFloat ButtonHeight;
  CGFloat ButtonHorizontalPadding;
  CGFloat ButtonTopPadding;
  CGFloat ButtonBottomPadding;
};

const AuthenticationViewConstants kCompactConstants = {
    24,  // PrimaryFontSize
    14,  // SecondaryFontSize
    40,  // GradientHeight
    36,  // ButtonHeight
    16,  // ButtonHorizontalPadding
    16,  // ButtonTopPadding
    16,  // ButtonBottomPadding
};

const AuthenticationViewConstants kRegularConstants = {
    1.5 * kCompactConstants.PrimaryFontSize,
    1.5 * kCompactConstants.SecondaryFontSize,
    kCompactConstants.GradientHeight,
    1.5 * kCompactConstants.ButtonHeight,
    32,
    32,
    32,
};

enum AuthenticationState {
  // Initial state.
  NULL_STATE,
  // Unified consent:
  //   Shows UnifiedConsentUserController
  // Non unified consent:
  //   Shows SigninAccountSelectorViewController
  // Lets the user add an account and choose an identity. Once the dialog is
  // validated, the state transitions to SIGNIN_PENDING_STATE to sign in.
  IDENTITY_PICKER_STATE,
  // Signs in using AuthenticationFlow. If it fails, the state transitions back
  // to IDENTITY_PICKER_STATE.
  // Unified consent:
  //   When done, transitions to DONE_STATE.
  // Non unified consent:
  //   When done, transitions to IDENTITY_SELECTED_STATE.
  SIGNIN_PENDING_STATE,
  // Used only for non unified consent. Shows the confirmation dialog with
  // SigninConfirmationViewController.
  IDENTITY_SELECTED_STATE,
  DONE_STATE,
};

}  // namespace

@interface ChromeSigninViewController ()<
    ChromeIdentityInteractionManagerDelegate,
    ChromeIdentityServiceObserver,
    MDCActivityIndicatorDelegate,
    SigninAccountSelectorViewControllerDelegate,
    UnifiedConsentCoordinatorDelegate>
@property(nonatomic, strong) ChromeIdentity* selectedIdentity;

@end

@implementation ChromeSigninViewController {
  ios::ChromeBrowserState* _browserState;  // weak
  __weak id<ChromeSigninViewControllerDelegate> _delegate;
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;
  ChromeIdentity* _selectedIdentity;
  TimerGeneratorBlock _timerGenerator;
  BOOL _unifiedConsentEnabled;

  // Authentication
  AlertCoordinator* _alertCoordinator;
  AuthenticationFlow* _authenticationFlow;
  BOOL _addedAccount;
  BOOL _autoSignIn;
  BOOL _didSignIn;
  BOOL _didAcceptSignIn;
  BOOL _didFinishSignIn;
  signin_metrics::AccessPoint _accessPoint;
  signin_metrics::PromoAction _promoAction;
  ChromeIdentityInteractionManager* _interactionManager;

  // Basic state.
  AuthenticationState _currentState;
  BOOL _ongoingStateChange;
  MDCActivityIndicator* _activityIndicator;
  MDCButton* _primaryButton;
  MDCButton* _secondaryButton;
  UIView* _gradientView;
  CAGradientLayer* _gradientLayer;
  UIView* _embeddedView;

  // Identity picker state.
  SigninAccountSelectorViewController* _accountSelectorVC;
  UnifiedConsentCoordinator* _unifiedConsentCoordinator;

  // Signin pending state.
  AuthenticationState _activityIndicatorNextState;
  std::unique_ptr<base::ElapsedTimer> _pendingStateTimer;
  std::unique_ptr<base::OneShotTimer> _leavingPendingStateTimer;

  // Identity selected state.
  SigninConfirmationViewController* _confirmationVC;
  BOOL _hasConfirmationScreenReachedBottom;
}

@synthesize delegate = _delegate;
@synthesize shouldClearData = _shouldClearData;
@synthesize dispatcher = _dispatcher;
@synthesize confirmationVC = _confirmationVC;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                         accessPoint:(signin_metrics::AccessPoint)accessPoint
                         promoAction:(signin_metrics::PromoAction)promoAction
                      signInIdentity:(ChromeIdentity*)identity
                          dispatcher:(id<ApplicationCommands>)dispatcher {
  self = [super init];
  if (self) {
    _unifiedConsentEnabled = unified_consent::IsUnifiedConsentFeatureEnabled();
    _browserState = browserState;
    _accessPoint = accessPoint;
    _promoAction = promoAction;
    _dispatcher = dispatcher;

    if (identity) {
      _autoSignIn = !_unifiedConsentEnabled;
      [self setSelectedIdentity:identity];
    }
    _identityServiceObserver.reset(
        new ChromeIdentityServiceObserverBridge(self));
    _currentState = NULL_STATE;
  }
  return self;
}

- (void)dealloc {
  // The call to -[UIControl addTarget:action:forControlEvents:] is made just
  // after the creation of those objects, so if the objects are not nil, then
  // it is safe to call -[UIControl removeTarget:action:forControlEvents:].
  // If they are nil, then the call does nothing.
  [_primaryButton removeTarget:self
                        action:@selector(onPrimaryButtonPressed:)
              forControlEvents:UIControlEventTouchDown];
  [_secondaryButton removeTarget:self
                          action:@selector(onSecondaryButtonPressed:)
                forControlEvents:UIControlEventTouchDown];
}

- (void)cancel {
  if (_alertCoordinator) {
    DCHECK(!_authenticationFlow && !_interactionManager);
    [_alertCoordinator executeCancelHandler];
    [_alertCoordinator stop];
  }
  if (_interactionManager) {
    DCHECK(!_alertCoordinator && !_authenticationFlow);
    [_interactionManager cancelAndDismissAnimated:NO];
  }
  if (_authenticationFlow) {
    DCHECK(!_alertCoordinator && !_interactionManager);
    [_authenticationFlow cancelAndDismiss];
  }
  if (!_didAcceptSignIn && _didSignIn) {
    AuthenticationServiceFactory::GetForBrowserState(_browserState)
        ->SignOut(signin_metrics::ABORT_SIGNIN, nil);
    _didSignIn = NO;
  }
  if (!_didFinishSignIn) {
    _didFinishSignIn = YES;
    [_delegate didFailSignIn:self];
  }
}

- (void)acceptSignInAndShowAccountsSettings:(BOOL)showAccountsSettings {
  signin_metrics::LogSigninAccessPointCompleted(_accessPoint, _promoAction);
  std::vector<int> consent_text_ids;
  int openSettingsStringId = -1;
  if (_unifiedConsentEnabled) {
    DCHECK(_unifiedConsentCoordinator);
    DCHECK(!_confirmationVC);
    consent_text_ids = _unifiedConsentCoordinator.consentStringIds;
    openSettingsStringId = _unifiedConsentCoordinator.openSettingsStringId;
  } else {
    DCHECK(_confirmationVC);
    DCHECK(!_unifiedConsentCoordinator);
    consent_text_ids = _confirmationVC.consentStringIds;
    openSettingsStringId = _confirmationVC.openSettingsStringId;
  }
  int consent_confirmation_id = showAccountsSettings
                                    ? openSettingsStringId
                                    : [self acceptSigninButtonStringId];
  std::string account_id =
      ios::AccountTrackerServiceFactory::GetForBrowserState(_browserState)
          ->PickAccountIdForAccount(
              base::SysNSStringToUTF8([_selectedIdentity gaiaID]),
              base::SysNSStringToUTF8([_selectedIdentity userEmail]));

  sync_pb::UserConsentTypes::SyncConsent sync_consent;
  sync_consent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                              UserConsentTypes_ConsentStatus_GIVEN);
  sync_consent.set_confirmation_grd_id(consent_confirmation_id);
  for (int id : consent_text_ids) {
    sync_consent.add_description_grd_ids(id);
  }
  ConsentAuditorFactory::GetForBrowserState(_browserState)
      ->RecordSyncConsent(account_id, sync_consent);
  _didAcceptSignIn = YES;
  if (!_didFinishSignIn) {
    _didFinishSignIn = YES;
    [_delegate didAcceptSignIn:self showAccountsSettings:showAccountsSettings];
  }
}

- (void)acceptSignInAndCommitSyncChanges {
  DCHECK(_didSignIn);
  if (_unifiedConsentEnabled) {
    // The consent has to be given as soon as the user is signed in. Even when
    // they open the settings through the link.
    unified_consent::UnifiedConsentService* unifiedConsentService =
        UnifiedConsentServiceFactory::GetForBrowserState(_browserState);
    // |unifiedConsentService| may be null in unit tests.
    if (unifiedConsentService)
      unifiedConsentService->SetUnifiedConsentGiven(true);
  }
  SyncSetupServiceFactory::GetForBrowserState(_browserState)->CommitChanges();
  [self acceptSignInAndShowAccountsSettings:_unifiedConsentCoordinator
                                                .settingsLinkWasTapped];
}

- (void)setPrimaryButtonStyling:(MDCButton*)button {
  [button setBackgroundColor:[[MDCPalette cr_bluePalette] tint500]
                    forState:UIControlStateNormal];
  [button setImage:nil forState:UIControlStateNormal];
  [button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
  [button setUnderlyingColorHint:[UIColor blackColor]];
  [button setInkColor:[UIColor colorWithWhite:1 alpha:0.2f]];
}

- (void)setSecondaryButtonStyling:(MDCButton*)button {
  [button setBackgroundColor:self.backgroundColor
                    forState:UIControlStateNormal];
  [button setTitleColor:[[MDCPalette cr_bluePalette] tint500]
               forState:UIControlStateNormal];
  [button setUnderlyingColorHint:[UIColor whiteColor]];
  [button setInkColor:[UIColor colorWithWhite:0 alpha:0.06f]];
}

// Configures the primary button as the more button. This can be used in
// IDENTITY_PICKER_STATE or IDENTITY_SELECTED_STATE.
- (void)updatePrimaryButtonAsMoreButton {
  DCHECK(_currentState == IDENTITY_PICKER_STATE ||
         _currentState == IDENTITY_SELECTED_STATE)
      << "Unsupported current state: " << _currentState;
  NSString* primaryButtonTitle = l10n_util::GetNSString(
      IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SCROLL_BUTTON);
  [_primaryButton setTitle:primaryButtonTitle forState:UIControlStateNormal];
  UIImage* primaryButtomImage =
      [UIImage imageNamed:@"signin_confirmation_more"];
  [_primaryButton setImage:primaryButtomImage forState:UIControlStateNormal];
  [self setSecondaryButtonStyling:_primaryButton];
}

// Configures the primary button for consent validation. This can be used in
// IDENTITY_PICKER_STATE or IDENTITY_SELECTED_STATE.
- (void)updatePrimaryButtonAsConsentValidationButton {
  DCHECK(_currentState == IDENTITY_PICKER_STATE ||
         _currentState == IDENTITY_SELECTED_STATE)
      << "Unsupported current state: " << _currentState;
  [self setPrimaryButtonStyling:_primaryButton];
  [_primaryButton setTitle:[self acceptSigninButtonTitle]
                  forState:UIControlStateNormal];
  [self.view setNeedsLayout];
}

// Displays |viewController.view| above the primary and secondary button.
// -[ChromeSigninViewController removeEmbeddedViewController:] has to be called
// before calling this method again.
- (void)showEmbeddedViewController:(UIViewController*)viewController {
  DCHECK(viewController);
  DCHECK(!_embeddedView);
  [self addChildViewController:viewController];
  _embeddedView = viewController.view;
  _embeddedView.frame = self.view.bounds;
  [self.view insertSubview:viewController.view belowSubview:_primaryButton];
  [viewController didMoveToParentViewController:self];
}

// Removes the view previously added by -[ChromeSigninViewController
// showEmbeddedViewController:].
- (void)removeEmbeddedViewController:(UIViewController*)viewController {
  DCHECK(_embeddedView);
  DCHECK_EQ(_embeddedView, viewController.view);
  [viewController willMoveToParentViewController:nil];
  [_embeddedView removeFromSuperview];
  [viewController removeFromParentViewController];
  _embeddedView = nil;
}

- (void)updateLayout {
  AuthenticationViewConstants constants;
  if ([self.traitCollection horizontalSizeClass] ==
      UIUserInterfaceSizeClassRegular) {
    constants = kRegularConstants;
  } else {
    constants = kCompactConstants;
  }

  [self layoutButtons:constants];

  CGSize viewSize = self.view.bounds.size;
  CGFloat collectionViewHeight =
      _primaryButton.frame.origin.y - constants.ButtonTopPadding;
  CGRect collectionViewFrame =
      CGRectMake(0, 0, viewSize.width, collectionViewHeight);
  [_embeddedView setFrame:collectionViewFrame];

  // Layout the gradient view right above the buttons.
  CGFloat gradientOriginY = _primaryButton.frame.origin.y -
                            constants.ButtonTopPadding -
                            constants.GradientHeight;
  [_gradientView setFrame:CGRectMake(0, gradientOriginY, viewSize.width,
                                     constants.GradientHeight)];
  [_gradientLayer setFrame:[_gradientView bounds]];

  // Layout the activity indicator in the center of the view.
  CGRect bounds = self.view.bounds;
  [_activityIndicator
      setCenter:CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds))];
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  // Simulate a press on the secondary button.
  [self onSecondaryButtonPressed:self];
  return YES;
}

#pragma mark - Properties

- (ios::ChromeBrowserState*)browserState {
  return _browserState;
}

- (id<ChromeSigninViewControllerDelegate>)delegate {
  return _delegate;
}

- (UIColor*)backgroundColor {
  return _unifiedConsentEnabled ? [UIColor whiteColor]
                                : [[MDCPalette greyPalette] tint50];
}

- (NSString*)identityPickerTitle {
  return l10n_util::GetNSString(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_TITLE);
}

- (int)acceptSigninButtonStringId {
  return _unifiedConsentEnabled
             ? IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON
             : IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON;
}

- (NSString*)acceptSigninButtonTitle {
  return l10n_util::GetNSString([self acceptSigninButtonStringId]);
}

- (NSString*)skipSigninButtonTitle {
  return l10n_util::GetNSString(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
}

- (UIButton*)primaryButton {
  return _primaryButton;
}

- (UIButton*)secondaryButton {
  return _secondaryButton;
}

- (void)setSelectedIdentity:(ChromeIdentity*)identity {
  DCHECK(identity || (IDENTITY_PICKER_STATE == _currentState));
  _selectedIdentity = identity;
  _unifiedConsentCoordinator.selectedIdentity = identity;
}

- (ChromeIdentity*)selectedIdentity {
  return _selectedIdentity;
}

#pragma mark - Authentication

- (void)handleAuthenticationError:(NSError*)error {
  // Filter out cancel and errors handled internally by ChromeIdentity.
  if (!ShouldHandleSigninError(error)) {
    return;
  }
  _alertCoordinator = ErrorCoordinator(error, nil, self);
  [_alertCoordinator start];
}

- (void)signIntoIdentity:(ChromeIdentity*)identity {
  [_delegate willStartSignIn:self];
  DCHECK(!_authenticationFlow);
  _authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowserState:_browserState
                                              identity:identity
                                       shouldClearData:_shouldClearData
                                      postSignInAction:POST_SIGNIN_ACTION_NONE
                              presentingViewController:self];
  _authenticationFlow.dispatcher = self.dispatcher;
  __weak ChromeSigninViewController* weakSelf = self;
  [_authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf onAccountSigninCompletion:success];
  }];
}

- (void)openAuthenticationDialogAddIdentity {
  DCHECK(!_interactionManager);
  _interactionManager =
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->CreateChromeIdentityInteractionManager(_browserState, self);
  __weak ChromeSigninViewController* weakSelf = self;
  SigninCompletionCallback completion =
      ^(ChromeIdentity* identity, NSError* error) {
        [weakSelf addAccountCompletedWithIdentity:identity error:error];
      };
  [_delegate willStartAddAccount:self];
  [_interactionManager addAccountWithCompletion:completion];
}

// Sets the added identity as the selected identity (if no error), and signs in
// with this identity only if not using unified consent.
- (void)addAccountCompletedWithIdentity:(ChromeIdentity*)identity
                                  error:(NSError*)error {
  // ChromeIdentityInteractionManager is not used anymore at this point.
  _interactionManager = nil;

  if (error) {
    [self handleAuthenticationError:error];
    return;
  }
  [self identityListChanged];
  [self setSelectedIdentity:identity];
  if (!_unifiedConsentEnabled) {
    _addedAccount = YES;
    [self changeToState:SIGNIN_PENDING_STATE];
  }
}

- (void)onAccountSigninCompletion:(BOOL)success {
  _authenticationFlow = nil;
  if (success) {
    DCHECK(!_didSignIn);
    _didSignIn = YES;
    [_delegate didSignIn:self];
    if (_unifiedConsentEnabled) {
      [self acceptSignInAndCommitSyncChanges];
    } else {
      [self changeToState:IDENTITY_SELECTED_STATE];
    }
  } else {
    [self changeToState:IDENTITY_PICKER_STATE];
  }
}

- (void)undoSignIn {
  if (_didSignIn) {
    AuthenticationServiceFactory::GetForBrowserState(_browserState)
        ->SignOut(signin_metrics::ABORT_SIGNIN, nil);
    [_delegate didUndoSignIn:self identity:self.selectedIdentity];
    _didSignIn = NO;
  }
  if (_addedAccount) {
    // This is best effort. If the operation fails, the account will be left on
    // the device. The user will not be warned either as this call is
    // asynchronous (but undo is not), the application might be in an unknown
    // state when the forget identity operation finishes.
    ios::GetChromeBrowserProvider()->GetChromeIdentityService()->ForgetIdentity(
        self.selectedIdentity, nil);
  }
  _addedAccount = NO;
}

#pragma mark - State machine

- (void)enterState:(AuthenticationState)state {
  _ongoingStateChange = NO;
  if (_didFinishSignIn) {
    // Stop the state machine when the sign-in is done.
    _currentState = DONE_STATE;
    return;
  }
  _currentState = state;
  switch (state) {
    case NULL_STATE:
      NOTREACHED();
      break;
    case IDENTITY_PICKER_STATE:
      [self enterIdentityPickerState];
      break;
    case SIGNIN_PENDING_STATE:
      [self enterSigninPendingState];
      break;
    case IDENTITY_SELECTED_STATE:
      [self enterIdentitySelectedState];
      break;
    case DONE_STATE:
      break;
  }
}

- (void)changeToState:(AuthenticationState)nextState {
  CHECK(nextState != IDENTITY_SELECTED_STATE || !_unifiedConsentEnabled);
  if (_currentState == nextState)
    return;
  _ongoingStateChange = YES;
  switch (_currentState) {
    case NULL_STATE:
      DCHECK_NE(IDENTITY_SELECTED_STATE, nextState);
      [self enterState:nextState];
      return;
    case IDENTITY_PICKER_STATE:
      DCHECK_EQ(SIGNIN_PENDING_STATE, nextState);
      [self leaveIdentityPickerState:nextState];
      return;
    case SIGNIN_PENDING_STATE:
      [self leaveSigninPendingState:nextState];
      return;
    case IDENTITY_SELECTED_STATE:
      DCHECK_EQ(IDENTITY_PICKER_STATE, nextState);
      [self leaveIdentitySelectedState:nextState];
      return;
    case DONE_STATE:
      // Ignored
      return;
  }
  NOTREACHED();
}

#pragma mark - IdentityPickerState

// Updates the primary button for IDENTITY_PICKER_STATE to be either the
// more button, the consent validation or sign-in button.
- (void)updatePrimaryButtonForIdentityPickerState {
  DCHECK_EQ(IDENTITY_PICKER_STATE, _currentState);
  if (_unifiedConsentEnabled) {
    if (!_unifiedConsentCoordinator.selectedIdentity) {
      [_primaryButton setTitle:l10n_util::GetNSString(
                                   IDS_IOS_ACCOUNT_UNIFIED_CONSENT_ADD_ACCOUNT)
                      forState:UIControlStateNormal];
      [self setPrimaryButtonStyling:_primaryButton];
    } else if (!_hasConfirmationScreenReachedBottom) {
      [self updatePrimaryButtonAsMoreButton];
    } else {
      [self updatePrimaryButtonAsConsentValidationButton];
    }
  } else {
    bool hasIdentities = ios::GetChromeBrowserProvider()
                             ->GetChromeIdentityService()
                             ->HasIdentities();
    NSString* primaryButtonTitle =
        hasIdentities
            ? l10n_util::GetNSString(
                  IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON)
            : l10n_util::GetNSString(
                  IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_NO_ACCOUNT_BUTTON);
    [_primaryButton setTitle:primaryButtonTitle forState:UIControlStateNormal];
    [self setPrimaryButtonStyling:_primaryButton];
  }
  [self.view setNeedsLayout];
}

- (void)enterIdentityPickerState {
  // Add the account selector view controller.
  if (_unifiedConsentEnabled) {
    if (!_unifiedConsentCoordinator) {
      // The user can refuse to sign-in into a managed account, so the state
      // returns to "IdentityPicker". In that case, there is no need to create a
      // new UnifiedConsentCoordinator. The current one should be used.
      _unifiedConsentCoordinator = [[UnifiedConsentCoordinator alloc] init];
      _unifiedConsentCoordinator.interactable = YES;
      _unifiedConsentCoordinator.delegate = self;
      if (_selectedIdentity)
        _unifiedConsentCoordinator.selectedIdentity = _selectedIdentity;
      [_unifiedConsentCoordinator start];
      [self
          showEmbeddedViewController:_unifiedConsentCoordinator.viewController];
    }
    DCHECK_EQ(_embeddedView, _unifiedConsentCoordinator.viewController.view);
  } else {
    // Reset the selected identity.
    [self setSelectedIdentity:nil];
    _accountSelectorVC = [[SigninAccountSelectorViewController alloc] init];
    _accountSelectorVC.delegate = self;
    [self showEmbeddedViewController:_accountSelectorVC];
  }

  // Update the button title.
  [self updatePrimaryButtonForIdentityPickerState];
  [_secondaryButton setTitle:self.skipSigninButtonTitle
                    forState:UIControlStateNormal];
  [self.view setNeedsLayout];
  _primaryButton.hidden = YES;
  _secondaryButton.hidden = YES;
  [UIView transitionWithView:_primaryButton
                    duration:kAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    _primaryButton.hidden = NO;
                  }
                  completion:nil];
  [UIView transitionWithView:_secondaryButton
                    duration:kAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    _secondaryButton.hidden = NO;
                  }
                  completion:nil];
}

- (void)reloadIdentityPickerState {
  // The account selector view controller reloads itself each time the list
  // of identities changes, thus there is no need to reload it.

  [self updatePrimaryButtonForIdentityPickerState];
}

- (void)leaveIdentityPickerState:(AuthenticationState)nextState {
  [UIView transitionWithView:_primaryButton
                    duration:kAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    _primaryButton.hidden = YES;
                  }
                  completion:nil];
  [UIView transitionWithView:_secondaryButton
      duration:kAnimationDuration
      options:UIViewAnimationOptionTransitionCrossDissolve
      animations:^{
        _secondaryButton.hidden = YES;
      }
      completion:^(BOOL finished) {
        // When the unified consent is enabled, the |_unifiedConsentVC| has to
        // be kept, so the consent can be recorded (with the string ids), once
        // the sign-in is done.
        if (!_unifiedConsentEnabled) {
          [self removeEmbeddedViewController:_accountSelectorVC];
          _accountSelectorVC = nil;
        }
        [self enterState:nextState];
      }];
}

#pragma mark - SigninPendingState

- (void)enterSigninPendingState {
  [_secondaryButton setTitle:l10n_util::GetNSString(IDS_CANCEL)
                    forState:UIControlStateNormal];
  [self.view setNeedsLayout];

  _pendingStateTimer.reset(new base::ElapsedTimer());
  _secondaryButton.hidden = NO;
  [_activityIndicator startAnimating];

  [self signIntoIdentity:self.selectedIdentity];
}

- (void)reloadSigninPendingState {
  BOOL isSelectedIdentityValid = ios::GetChromeBrowserProvider()
                                     ->GetChromeIdentityService()
                                     ->IsValidIdentity(self.selectedIdentity);
  if (!isSelectedIdentityValid) {
    [_authenticationFlow cancelAndDismiss];
    [self changeToState:IDENTITY_PICKER_STATE];
  }
}

- (void)leaveSigninPendingState:(AuthenticationState)nextState {
  if (!_pendingStateTimer) {
    // The controller is already leaving the signin pending state, simply update
    // the new state to take into account the last request only.
    _activityIndicatorNextState = nextState;
    return;
  }

  _activityIndicatorNextState = nextState;
  _activityIndicator.delegate = self;

  base::TimeDelta remainingTime =
      base::TimeDelta::FromMilliseconds(kMinimunPendingStateDurationMs) -
      _pendingStateTimer->Elapsed();
  _pendingStateTimer.reset();

  if (remainingTime.InMilliseconds() < 0) {
    [_activityIndicator stopAnimating];
  } else {
    // If the signin pending state is too fast, the screen will appear to
    // flicker. Make sure to animate for at least
    // |kMinimunPendingStateDurationMs| milliseconds.
    __weak ChromeSigninViewController* weakSelf = self;
    ProceduralBlock completionBlock = ^{
      ChromeSigninViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return;
      [strongSelf->_activityIndicator stopAnimating];
      strongSelf->_leavingPendingStateTimer.reset();
    };
    if (self.timerGenerator) {
      _leavingPendingStateTimer = self.timerGenerator();
      DCHECK(_leavingPendingStateTimer);
    } else {
      _leavingPendingStateTimer = std::make_unique<base::OneShotTimer>();
    }
    _leavingPendingStateTimer->Start(FROM_HERE, remainingTime,
                                     base::BindRepeating(completionBlock));
  }
}

#pragma mark - IdentitySelectedState

// Updates the primary button for IDENTITY_SELECTED_STATE to be either the
// more button or the consent validation.
- (void)updatePrimaryButtonForIdentitySelectedState {
  DCHECK_EQ(IDENTITY_SELECTED_STATE, _currentState);
  DCHECK(!_unifiedConsentEnabled);
  if (_hasConfirmationScreenReachedBottom) {
    [self updatePrimaryButtonAsConsentValidationButton];
  } else {
    [self updatePrimaryButtonAsMoreButton];
  }
  [self.view setNeedsLayout];
}

- (void)enterIdentitySelectedState {
  _confirmationVC = [[SigninConfirmationViewController alloc]
      initWithIdentity:self.selectedIdentity];
  _confirmationVC.delegate = self;

  _hasConfirmationScreenReachedBottom = NO;
  [self showEmbeddedViewController:_confirmationVC];

  [self updatePrimaryButtonForIdentitySelectedState];
  NSString* secondaryButtonTitle = l10n_util::GetNSString(
      IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_UNDO_BUTTON);
  [_secondaryButton setTitle:secondaryButtonTitle
                    forState:UIControlStateNormal];
  [self.view setNeedsLayout];
  _primaryButton.hidden = YES;
  _secondaryButton.hidden = YES;
  [UIView transitionWithView:_primaryButton
                    duration:kAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    _primaryButton.hidden = NO;
                  }
                  completion:nil];
  [UIView transitionWithView:_secondaryButton
                    duration:kAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    _secondaryButton.hidden = NO;
                  }
                  completion:nil];
}

- (void)reloadIdentitySelectedState {
  BOOL isSelectedIdentityValid = ios::GetChromeBrowserProvider()
                                     ->GetChromeIdentityService()
                                     ->IsValidIdentity(self.selectedIdentity);
  if (!isSelectedIdentityValid) {
    [self changeToState:IDENTITY_PICKER_STATE];
    return;
  }
}

- (void)leaveIdentitySelectedState:(AuthenticationState)nextState {
  [self removeEmbeddedViewController:_confirmationVC];
  _confirmationVC = nil;
  [self setPrimaryButtonStyling:_primaryButton];
  _primaryButton.hidden = YES;
  _secondaryButton.hidden = YES;
  [self undoSignIn];
  [self enterState:nextState];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = self.backgroundColor;

  _primaryButton = [[MDCFlatButton alloc] init];
  [self setPrimaryButtonStyling:_primaryButton];
  [_primaryButton addTarget:self
                     action:@selector(onPrimaryButtonPressed:)
           forControlEvents:UIControlEventTouchUpInside];
  _primaryButton.hidden = YES;
  [self.view addSubview:_primaryButton];

  _secondaryButton = [[MDCFlatButton alloc] init];
  [self setSecondaryButtonStyling:_secondaryButton];
  [_secondaryButton addTarget:self
                       action:@selector(onSecondaryButtonPressed:)
             forControlEvents:UIControlEventTouchUpInside];
  [_secondaryButton setAccessibilityIdentifier:@"ic_close"];
  _secondaryButton.hidden = YES;
  [self.view addSubview:_secondaryButton];

  _activityIndicator = [[MDCActivityIndicator alloc] initWithFrame:CGRectZero];
  [_activityIndicator setDelegate:self];
  [_activityIndicator setStrokeWidth:3];
  [_activityIndicator
      setCycleColors:@[ [[MDCPalette cr_bluePalette] tint500] ]];
  [self.view addSubview:_activityIndicator];

  _gradientView = [[UIView alloc] initWithFrame:CGRectZero];
  _gradientLayer = [CAGradientLayer layer];
  [_gradientView setUserInteractionEnabled:NO];
  _gradientLayer.colors = [NSArray
      arrayWithObjects:(id)[[UIColor colorWithWhite:1 alpha:0] CGColor],
                       (id)[self.backgroundColor CGColor], nil];
  [[_gradientView layer] insertSublayer:_gradientLayer atIndex:0];
  [self.view addSubview:_gradientView];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  if (_currentState != NULL_STATE) {
    return;
  }
  if (_autoSignIn) {
    [self enterState:SIGNIN_PENDING_STATE];
  } else {
    [self enterState:IDENTITY_PICKER_STATE];
  }
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  [self updateLayout];
}

#pragma mark - Events

- (void)onPrimaryButtonPressed:(id)sender {
  switch (_currentState) {
    case NULL_STATE:
      NOTREACHED();
      return;
    case IDENTITY_PICKER_STATE: {
      if (_unifiedConsentEnabled) {
        if (!_unifiedConsentCoordinator.selectedIdentity) {
          [self openAuthenticationDialogAddIdentity];
        } else if (!_hasConfirmationScreenReachedBottom) {
          [_unifiedConsentCoordinator scrollToBottom];
        } else {
          ChromeIdentity* selectedIdentity =
              _unifiedConsentCoordinator.selectedIdentity;
          [self setSelectedIdentity:selectedIdentity];
          [self changeToState:SIGNIN_PENDING_STATE];
        }
      } else {
        if (_interactionManager) {
          // Adding an account is ongoing, ignore the button press.
          return;
        }
        ChromeIdentity* selectedIdentity =
            [_accountSelectorVC selectedIdentity];
        [self setSelectedIdentity:selectedIdentity];
        if (selectedIdentity) {
          [self changeToState:SIGNIN_PENDING_STATE];
        } else {
          [self openAuthenticationDialogAddIdentity];
        }
      }
      return;
    }
    case SIGNIN_PENDING_STATE:
      NOTREACHED();
      return;
    case IDENTITY_SELECTED_STATE:
      if (_hasConfirmationScreenReachedBottom) {
        [self acceptSignInAndCommitSyncChanges];
      } else {
        [_confirmationVC scrollToBottom];
      }
      return;
    case DONE_STATE:
      // Ignored
      return;
  }
  NOTREACHED();
}

- (void)onSecondaryButtonPressed:(id)sender {
  switch (_currentState) {
    case NULL_STATE:
      NOTREACHED();
      return;
    case IDENTITY_PICKER_STATE:
      if (!_didFinishSignIn) {
        _didFinishSignIn = YES;
        [_delegate didSkipSignIn:self];
      }
      return;
    case SIGNIN_PENDING_STATE:
      base::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
      [_authenticationFlow cancelAndDismiss];
      [self undoSignIn];
      [self changeToState:IDENTITY_PICKER_STATE];
      return;
    case IDENTITY_SELECTED_STATE:
      base::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
      [self changeToState:IDENTITY_PICKER_STATE];
      return;
    case DONE_STATE:
      // Ignored
      return;
  }
  NOTREACHED();
}

#pragma mark - ChromeIdentityServiceObserver

- (void)identityListChanged {
  switch (_currentState) {
    case NULL_STATE:
    case DONE_STATE:
      return;
    case IDENTITY_PICKER_STATE:
      [self reloadIdentityPickerState];
      return;
    case SIGNIN_PENDING_STATE:
      [self reloadSigninPendingState];
      return;
    case IDENTITY_SELECTED_STATE:
      [self reloadIdentitySelectedState];
      return;
  }
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

#pragma mark - Layout

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateLayout];
}

- (void)layoutButtons:(const AuthenticationViewConstants&)constants {
  UIFont* font =
      [[MDCTypography fontLoader] mediumFontOfSize:constants.SecondaryFontSize];
  [_primaryButton setTitleFont:font forState:UIControlStateNormal];
  [_secondaryButton setTitleFont:font forState:UIControlStateNormal];

  LayoutRect primaryButtonLayout = LayoutRectZero;
  primaryButtonLayout.boundingWidth = CGRectGetWidth(self.view.bounds);
  primaryButtonLayout.size = [_primaryButton
      sizeThatFits:CGSizeMake(CGFLOAT_MAX, constants.ButtonHeight)];
  primaryButtonLayout.position.leading = primaryButtonLayout.boundingWidth -
                                         primaryButtonLayout.size.width -
                                         constants.ButtonHorizontalPadding;
  primaryButtonLayout.position.originY = CGRectGetHeight(self.view.bounds) -
                                         constants.ButtonBottomPadding -
                                         constants.ButtonHeight;
  if (@available(iOS 11.0, *)) {
    primaryButtonLayout.position.originY -= self.view.safeAreaInsets.bottom;
  }
  primaryButtonLayout.size.height = constants.ButtonHeight;
  [_primaryButton setFrame:LayoutRectGetRect(primaryButtonLayout)];

  UIEdgeInsets imageInsets = UIEdgeInsetsZero;
  UIEdgeInsets titleInsets = UIEdgeInsetsZero;
  if ([_primaryButton imageForState:UIControlStateNormal]) {
    // Title label should be leading, followed by the image (with some padding).
    CGFloat paddedImageWidth =
        [_primaryButton imageView].frame.size.width + kMoreButtonPadding;
    CGFloat paddedTitleWidth =
        [_primaryButton titleLabel].frame.size.width + kMoreButtonPadding;
    imageInsets = UIEdgeInsetsMake(0, paddedTitleWidth, 0, -paddedTitleWidth);
    titleInsets = UIEdgeInsetsMake(0, -paddedImageWidth, 0, paddedImageWidth);
  }
  [_primaryButton setImageEdgeInsets:imageInsets];
  [_primaryButton setTitleEdgeInsets:titleInsets];

  LayoutRect secondaryButtonLayout = primaryButtonLayout;
  secondaryButtonLayout.size = [_secondaryButton
      sizeThatFits:CGSizeMake(CGFLOAT_MAX, constants.ButtonHeight)];
  secondaryButtonLayout.position.leading = constants.ButtonHorizontalPadding;
  secondaryButtonLayout.size.height = constants.ButtonHeight;
  [_secondaryButton setFrame:LayoutRectGetRect(secondaryButtonLayout)];
}

- (void)didReachBottom {
  if (_hasConfirmationScreenReachedBottom)
    return;
  _hasConfirmationScreenReachedBottom = YES;
  switch (_currentState) {
    case NULL_STATE:
    case DONE_STATE:
    case SIGNIN_PENDING_STATE:
      NOTREACHED();
      break;
    case IDENTITY_PICKER_STATE:
      [self updatePrimaryButtonForIdentityPickerState];
      break;
    case IDENTITY_SELECTED_STATE:
      [self updatePrimaryButtonForIdentitySelectedState];
      break;
  }
}

#pragma mark - MDCActivityIndicatorDelegate

- (void)activityIndicatorAnimationDidFinish:
    (MDCActivityIndicator*)activityIndicator {
  DCHECK_EQ(SIGNIN_PENDING_STATE, _currentState);
  DCHECK_EQ(_activityIndicator, activityIndicator);

  // The activity indicator is only used in the signin pending state. Its
  // animation is stopped only when leaving the state.
  if (_activityIndicatorNextState != NULL_STATE) {
    [self enterState:_activityIndicatorNextState];
    _activityIndicatorNextState = NULL_STATE;
  }
}

#pragma mark - ChromeIdentityInteractionManagerDelegate

- (void)interactionManager:(ChromeIdentityInteractionManager*)interactionManager
     presentViewController:(UIViewController*)viewController
                  animated:(BOOL)animated
                completion:(ProceduralBlock)completion {
  [self presentViewController:viewController
                     animated:animated
                   completion:completion];
}

- (void)interactionManager:(ChromeIdentityInteractionManager*)interactionManager
    dismissViewControllerAnimated:(BOOL)animated
                       completion:(ProceduralBlock)completion {
  [self dismissViewControllerAnimated:animated completion:completion];
}

#pragma mark - SigninAccountSelectorViewControllerDelegate

- (void)accountSelectorControllerDidSelectAddAccount:
    (SigninAccountSelectorViewController*)accountSelectorController {
  DCHECK_EQ(_accountSelectorVC, accountSelectorController);
  if (_ongoingStateChange) {
    return;
  }
  [self openAuthenticationDialogAddIdentity];
}

#pragma mark - SigninConfirmationViewControllerDelegate

// Callback for when a link in the label is pressed.
- (void)signinConfirmationControllerDidTapSettingsLink:
    (SigninConfirmationViewController*)controller {
  DCHECK_EQ(_confirmationVC, controller);

  [self acceptSignInAndShowAccountsSettings:YES];
}

- (void)signinConfirmationControllerDidReachBottom:
    (SigninConfirmationViewController*)controller {
  [self didReachBottom];
}

#pragma mark - UnifiedConsentCoordinatorDelegate

- (void)unifiedConsentCoordinatorDidTapSettingsLink:
    (UnifiedConsentCoordinator*)coordinator {
  DCHECK_EQ(_unifiedConsentCoordinator, coordinator);
  DCHECK_EQ(IDENTITY_PICKER_STATE, _currentState);
  ChromeIdentity* selectedIdentity =
      _unifiedConsentCoordinator.selectedIdentity;
  [self setSelectedIdentity:selectedIdentity];
  if (selectedIdentity) {
    [self changeToState:SIGNIN_PENDING_STATE];
  } else {
    [self openAuthenticationDialogAddIdentity];
  }
}

- (void)unifiedConsentCoordinatorDidReachBottom:
    (UnifiedConsentCoordinator*)coordinator {
  DCHECK_EQ(_unifiedConsentCoordinator, coordinator);
  DCHECK_EQ(IDENTITY_PICKER_STATE, _currentState);
  [self didReachBottom];
}

- (void)unifiedConsentCoordinatorDidTapOnAddAccount:
    (UnifiedConsentCoordinator*)coordinator {
  DCHECK_EQ(_unifiedConsentCoordinator, coordinator);
  [self openAuthenticationDialogAddIdentity];
}

@end

@implementation ChromeSigninViewController (Testing)

- (TimerGeneratorBlock)timerGenerator {
  return _timerGenerator;
}

- (void)setTimerGenerator:(TimerGeneratorBlock)timerGenerator {
  _timerGenerator = [timerGenerator copy];
}

@end
