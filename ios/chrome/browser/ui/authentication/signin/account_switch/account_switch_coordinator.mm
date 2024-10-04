// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/account_switch/account_switch_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet/signout_action_sheet_coordinator.h"

using signin_metrics::AccessPoint;

@interface AccountSwitchCoordinator () {
  // Signout action sheet coordinator, to handle the signout part of account
  // switching.
  SignoutActionSheetCoordinator* _signoutActionSheetCoordinator;

  // Authentication flow, to handle the signin part of account switching.
  AuthenticationFlow* _authenticationFlow;

  // Identity to switch from.
  id<SystemIdentity> _fromIdentity;

  // Identity to sign into.
  id<SystemIdentity> _newIdentity;

  // Browser.
  raw_ptr<Browser> _browser;

  // BaseViewController to present the signout dialogs on top of it.
  UIViewController* _baseViewController;

  // ViewController to present the signin dialogs on top of it.
  UIViewController* _mainViewController;

  // Rect.
  CGRect _rect;

  // Anchor view for _rect.
  UIView* _rectAnchorView;

  // This object is set iff an account switch is in progress.
  base::ScopedClosureRunner _accountSwitchInProgress;

  // Authentication Service.
  raw_ptr<AuthenticationService> _authenticationService;

  // Completion block to call once AuthenticationFlow is done while being
  // interrupted.
  ProceduralBlock _interruptionCompletion;

  // Callback to execute when there we know the user won’t have any more
  // opportunity to cancel.
  void (^_userDecisionCompletion)();
}

@end

@implementation AccountSwitchCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                               newIdentity:(id<SystemIdentity>)newIdentity
                        mainViewController:(UIViewController*)mainViewController
                                      rect:(CGRect)rect
                    userDecisionCompletion:(void (^)())userDecisionCompletion
                            rectAnchorView:(UIView*)rectAnchorView {
  self = [super initWithBaseViewController:baseViewController
                                   browser:browser
                               accessPoint:signin_metrics::AccessPoint::
                                               ACCESS_POINT_ACCOUNT_MENU];
  if (self) {
    _browser = browser;
    _baseViewController = baseViewController;
    _mainViewController = mainViewController;
    _rect = rect;
    _rectAnchorView = rectAnchorView;
    _newIdentity = newIdentity;
    _userDecisionCompletion = userDecisionCompletion;

    ProfileIOS* profile = _browser->GetProfile();
    _authenticationService =
        AuthenticationServiceFactory::GetForProfile(profile);
    _fromIdentity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
  }
  return self;
}

#pragma mark - AccountSwitchCoordinator

- (void)start {
  [super start];
  CHECK(_baseViewController);
  CHECK(_mainViewController);
  CHECK(_fromIdentity);
  CHECK(_newIdentity);
  [self startAccountSwitch];
}

- (void)stop {
  [super stop];
  _authenticationService = nullptr;
  _authenticationFlow = nil;
  _accountSwitchInProgress.RunAndReset();
  [self stopSignoutActionSheetCoordinator];
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  if (_signoutActionSheetCoordinator) {
    CHECK(!_authenticationFlow);
    [self stopSignoutActionSheetCoordinator];
    SigninCompletionInfo* completionInfo =
        [SigninCompletionInfo signinCompletionInfoWithIdentity:nil];
    [self
        runCompletionCallbackWithSigninResult:SigninCoordinatorResultInterrupted
                               completionInfo:completionInfo];
    if (completion) {
      completion();
    }
    return;
  }
  CHECK(_authenticationFlow);
  _interruptionCompletion = [completion copy];
  [_authenticationFlow interruptWithAction:action];
}

#pragma mark - Private

- (void)startAccountSwitch {
  _accountSwitchInProgress =
      _authenticationService->DeclareAccountSwitchInProgress();
  _signoutActionSheetCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                            rect:_rect
                            view:_rectAnchorView
        forceSnackbarOverToolbar:YES
                      withSource:signin_metrics::ProfileSignout::
                                     kChangeAccountInAccountMenu];
  _signoutActionSheetCoordinator.accountSwitch = YES;

  __weak __typeof(self) weakSelf = self;
  _signoutActionSheetCoordinator.signoutCompletion = ^(BOOL success) {
    [weakSelf signoutDoneCompletionWithSuccess:success];
  };
  [_signoutActionSheetCoordinator start];
}

- (void)triggerSignin {
  _authenticationFlow = [[AuthenticationFlow alloc]
               initWithBrowser:_browser
                      identity:_newIdentity
                   accessPoint:signin_metrics::AccessPoint::
                                   ACCESS_POINT_ACCOUNT_MENU
             postSignInActions:PostSignInActionSet({PostSignInAction::kNone})
      presentingViewController:_mainViewController];

  __weak __typeof(self) weakSelf = self;
  _authenticationFlow.userDecisionCompletion = _userDecisionCompletion;
  [_authenticationFlow
      startSignInWithCompletion:^(SigninCoordinatorResult result) {
        [weakSelf signinDoneWithResult:result];
      }];
}

- (void)triggerAccountSwitchSnackbarWithIdentity {
  ProfileIOS* profile = _browser->GetProfile();

  UIImage* avatar = ChromeAccountManagerServiceFactory::GetForProfile(profile)
                        ->GetIdentityAvatarWithIdentity(
                            _newIdentity, IdentityAvatarSize::Regular);
  PrefService* prefService = profile->GetPrefs();
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  ManagementState managementState =
      GetManagementState(identityManager, _authenticationService, prefService);

  MDCSnackbarMessage* snackbarTitle = [[IdentitySnackbarMessage alloc]
      initWithName:_newIdentity.userGivenName
             email:_newIdentity.userEmail
            avatar:avatar
           managed:managementState.is_profile_managed()];
  CommandDispatcher* dispatcher = _browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbarCommandsHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbarCommandsHandler showSnackbarMessageOverBrowserToolbar:snackbarTitle];
}

- (void)signoutDoneCompletionWithSuccess:(BOOL)success {
  [self stopSignoutActionSheetCoordinator];
  if (success) {
    [self triggerSignin];
  } else {
    _accountSwitchInProgress.RunAndReset();
    SigninCompletionInfo* completionInfo =
        [SigninCompletionInfo signinCompletionInfoWithIdentity:nil];
    [self runCompletionCallbackWithSigninResult:
              SigninCoordinatorResult::SigninCoordinatorResultCanceledByUser
                                 completionInfo:completionInfo];
  }
}

- (void)signinDoneWithResult:(SigninCoordinatorResult)signinResult {
  SigninCompletionInfo* completionInfo = nil;
  switch (signinResult) {
    case SigninCoordinatorResult::SigninCoordinatorResultSuccess:
      [self triggerAccountSwitchSnackbarWithIdentity];
      completionInfo =
          [SigninCompletionInfo signinCompletionInfoWithIdentity:_newIdentity];
      break;
    case SigninCoordinatorResult::SigninCoordinatorResultInterrupted:
    case SigninCoordinatorResult::SigninCoordinatorResultCanceledByUser:
    case SigninCoordinatorResult::SigninCoordinatorResultDisabled:
      ProfileIOS* profile = _browser->GetProfile();
      if (ChromeAccountManagerServiceFactory::GetForProfile(profile)
              ->IsValidIdentity(_fromIdentity)) {
        // Sign-in to new identity failed, we sign the user back in their
        // previous account.
        _authenticationService->SignIn(
            _fromIdentity, signin_metrics::AccessPoint::
                               ACCESS_POINT_ACCOUNT_MENU_FAILED_SWITCH);
      }
      completionInfo =
          [SigninCompletionInfo signinCompletionInfoWithIdentity:nil];
      break;
  }

  [self runCompletionCallbackWithSigninResult:signinResult
                               completionInfo:completionInfo];
  _accountSwitchInProgress.RunAndReset();
  if (_interruptionCompletion) {
    _interruptionCompletion();
  }
}

- (void)stopSignoutActionSheetCoordinator {
  [_signoutActionSheetCoordinator stop];
  _signoutActionSheetCoordinator = nil;
}

@end
