// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/instant_signin/instant_signin_coordinator.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_coordinator.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/signin/instant_signin/instant_signin_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator_delegate.h"

@interface InstantSigninCoordinator () <AuthenticationFlowDelegate,
                                        IdentityChooserCoordinatorDelegate,
                                        InstantSigninMediatorDelegate>

@end

@implementation InstantSigninCoordinator {
  // The identity to which the user should be signed-in.
  id<SystemIdentity> _identity;
  // Access point where sign-in coordinator was triggered.
  signin_metrics::AccessPoint _accessPoint;
  // Instant sign-in mediator.
  InstantSigninMediator* _mediator;
  // Coordinator for the user to select an account.
  IdentityChooserCoordinator* _identityChooserCoordinator;
  // Coordinator to add an account.
  SigninCoordinator* _addAccountSigninCoordinator;
  // Overlay to block the current window while the sign-in is in progress.
  ActivityOverlayCoordinator* _activityOverlayCoordinator;
  // Whether a snackbar displaying the signed-in account and an "Undo" button
  // should be displayed after successful sign-in.
  BOOL _showSnackbarAfterSuccessfulSignin;
}

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  identity:(id<SystemIdentity>)identity
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _identity = identity;
    _accessPoint = accessPoint;
  }
  return self;
}

- (void)dealloc {
  CHECK(!_mediator);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  ChromeBrowserState* chromeState = self.browser->GetBrowserState();
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(chromeState);
  _mediator = [[InstantSigninMediator alloc] initWithSyncService:syncService
                                                     accessPoint:_accessPoint];
  _mediator.delegate = self;
  if (_identity) {
    // No other dialog will be shown in this flow, so display the snackbar to
    // ensure the full signed-in account is shown at least once.
    _showSnackbarAfterSuccessfulSignin = YES;
    // If an identity was selected, sign-in can start now.
    [self startSignInOnlyFlow];
    return;
  }

  // The remaining code paths already contain some UI that fully displays the
  // signed-in account, so no need for the snackbar. They happen to currently
  // show it, so guard the change behind flag.
  _showSnackbarAfterSuccessfulSignin =
      !base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos);

  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(chromeState);
  if (!accountManagerService->HasIdentities()) {
    [self startAddAccountForSignInOnly];
    return;
  }
  // Otherwise, the user needs to choose an identity.
  _identityChooserCoordinator = [[IdentityChooserCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser];
  _identityChooserCoordinator.delegate = self;
  [_identityChooserCoordinator start];
}

- (void)stop {
  CHECK(!_addAccountSigninCoordinator);
  CHECK(!_activityOverlayCoordinator);
  CHECK(!_identityChooserCoordinator);
  [_mediator disconnect];
  _mediator.delegate = nil;
  _mediator = nil;
  [super stop];
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  if (_addAccountSigninCoordinator) {
    CHECK(!_identityChooserCoordinator);
    CHECK(!_activityOverlayCoordinator);
    [_addAccountSigninCoordinator interruptWithAction:action
                                           completion:completion];
  } else if (_identityChooserCoordinator) {
    CHECK(!_activityOverlayCoordinator);
    [_identityChooserCoordinator stop];
    _identityChooserCoordinator = nil;
    [self
        runCompletionCallbackWithSigninResult:SigninCoordinatorResultInterrupted
                               completionInfo:nil];
    if (completion) {
      completion();
    }
  } else {
    [_mediator interruptWithAction:action completion:completion];
  }
}

#pragma mark - AuthenticationFlowDelegate

- (void)didPresentDialog {
  [self removeActivityOverlay];
}

- (void)didDismissDialog {
  [self showActivityOverlay];
}

#pragma mark - IdentityChooserCoordinatorDelegate

- (void)identityChooserCoordinatorDidClose:
    (IdentityChooserCoordinator*)coordinator {
  // `_identityChooserCoordinator.delegate` was set to nil before calling this
  // method since `identityChooserCoordinatorDidTapOnAddAccount:` or
  // `identityChooserCoordinator:didSelectIdentity:` have been called before.
  NOTREACHED() << base::SysNSStringToUTF8([self description]);
}

- (void)identityChooserCoordinatorDidTapOnAddAccount:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(coordinator, _identityChooserCoordinator)
      << base::SysNSStringToUTF8([self description]);
  _identityChooserCoordinator.delegate = nil;
  [_identityChooserCoordinator stop];
  _identityChooserCoordinator = nil;
  [self startAddAccountForSignInOnly];
}

- (void)identityChooserCoordinator:(IdentityChooserCoordinator*)coordinator
                 didSelectIdentity:(id<SystemIdentity>)identity {
  CHECK_EQ(coordinator, _identityChooserCoordinator)
      << base::SysNSStringToUTF8([self description]);
  _identityChooserCoordinator.delegate = nil;
  [_identityChooserCoordinator stop];
  _identityChooserCoordinator = nil;
  if (!identity) {
    // If no identity was selected, the coordinator can be closed.
    [self runCompletionCallbackWithSigninResult:
              SigninCoordinatorResultCanceledByUser
                                 completionInfo:nil];
    return;
  }
  _identity = identity;
  [_identityChooserCoordinator stop];
  _identityChooserCoordinator = nil;
  // The identity is now selected, the sign-in flow can be started.
  [self startSignInOnlyFlow];
}

#pragma mark - InstantSigninMediatorDelegate

- (void)instantSigninMediator:(InstantSigninMediator*)mediator
          didSigninWithResult:(SigninCoordinatorResult)result {
  [self removeActivityOverlay];
  switch (result) {
    case SigninCoordinatorResultSuccess: {
      SigninCompletionInfo* info =
          [SigninCompletionInfo signinCompletionInfoWithIdentity:_identity];
      [self runCompletionCallbackWithSigninResult:SigninCoordinatorResultSuccess
                                   completionInfo:info];
      break;
    }
    case SigninCoordinatorResultDisabled:
    case SigninCoordinatorResultInterrupted:
    case SigninCoordinatorResultCanceledByUser:
      [self runCompletionCallbackWithSigninResult:result completionInfo:nil];
      break;
  }
}

#pragma mark - Private

// Starts the sign-in flow.
- (void)startSignInOnlyFlow {
  [self showActivityOverlay];
  signin_metrics::RecordSigninUserActionForAccessPoint(_accessPoint);
  auto postSigninAction = _showSnackbarAfterSuccessfulSignin
                              ? PostSignInAction::kShowSnackbar
                              : PostSignInAction::kNone;
  AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:_identity
                                      accessPoint:_accessPoint
                                 postSignInAction:postSigninAction
                         presentingViewController:self.baseViewController];
  authenticationFlow.delegate = self;
  [_mediator startSignInOnlyFlowWithAuthenticationFlow:authenticationFlow];
}

// Starts the add account coordinator.
- (void)startAddAccountForSignInOnly {
  _addAccountSigninCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.baseViewController
                                          browser:self.browser
                                      accessPoint:_accessPoint];
  __weak __typeof(self) weakSelf = self;
  _addAccountSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* info) {
        [weakSelf addAccountDoneWithResult:result info:info];
      };
  [_addAccountSigninCoordinator start];
}

// Starts the sign-in flow if the identity has been selected, otherwise, it
// ends this coordinator.
- (void)addAccountDoneWithResult:(SigninCoordinatorResult)result
                            info:(SigninCompletionInfo*)info {
  CHECK(_addAccountSigninCoordinator)
      << base::SysNSStringToUTF8([self description]);
  _addAccountSigninCoordinator = nil;
  switch (result) {
    case SigninCoordinatorResultSuccess:
      _identity = info.identity;
      [self startSignInOnlyFlow];
      break;
    case SigninCoordinatorResultDisabled:
    case SigninCoordinatorResultInterrupted:
    case SigninCoordinatorResultCanceledByUser:
      [self runCompletionCallbackWithSigninResult:result completionInfo:nil];
      break;
  }
}

// Adds an activity overlay to block the UI.
- (void)showActivityOverlay {
  CHECK(!_activityOverlayCoordinator);
  _activityOverlayCoordinator = [[ActivityOverlayCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser];
  [_activityOverlayCoordinator start];
}

// Removes an activity overlay to block the UI. `-[HistorySyncCoordinator
// showActivityOverlay]` must have been called before.
- (void)removeActivityOverlay {
  CHECK(_activityOverlayCoordinator);
  [_activityOverlayCoordinator stop];
  _activityOverlayCoordinator = nil;
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"<%@: %p, identity: %p, accessPoint: %d, mediator %p, "
          @"identityChooserCoordinator %p, addAccountSigninCoordinator %p, "
          @"baseViewController: %@, window: %p>",
          self.class.description, self, _identity,
          static_cast<int>(_accessPoint), _mediator,
          _identityChooserCoordinator, _addAccountSigninCoordinator,
          NSStringFromClass([self.baseViewController class]),
          self.baseViewController.view.window];
}

@end
