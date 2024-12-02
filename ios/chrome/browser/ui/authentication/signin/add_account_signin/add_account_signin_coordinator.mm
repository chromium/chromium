// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_coordinator.h"

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_interaction_manager.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_popup_coordinator.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_manager.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@interface AddAccountSigninCoordinator () <AddAccountSigninManagerDelegate,
                                           HistorySyncPopupCoordinatorDelegate>

// Coordinator to display modal alerts to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
// Coordinator to handle additional steps after the identity is added, i.e.
// after `addAccountSigninManager` does its job.
@property(nonatomic, strong) SigninCoordinator* postSigninManagerCoordinator;
// Coordinator for history sync opt-in.
@property(nonatomic, strong)
    HistorySyncPopupCoordinator* historySyncPopupCoordinator;
// Manager that handles sign-in add account UI.
@property(nonatomic, strong) AddAccountSigninManager* addAccountSigninManager;
// Promo button used to trigger the sign-in.
@property(nonatomic, assign) PromoAction promoAction;
// Add account sign-in intent.
@property(nonatomic, assign, readonly) AddAccountSigninIntent signinIntent;

@end

@implementation AddAccountSigninCoordinator {
  // Account manager service to retrieve Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // Identity manager to retrieve Chrome identities.
  raw_ptr<signin::IdentityManager> _identityManager;
  raw_ptr<AuthenticationService> _authenticationService;
  raw_ptr<syncer::SyncService> _syncService;
}

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               accessPoint:(AccessPoint)accessPoint
                               promoAction:(PromoAction)promoAction
                              signinIntent:
                                  (AddAccountSigninIntent)signinIntent {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                               accessPoint:accessPoint];
  if (self) {
    _signinIntent = signinIntent;
    _promoAction = promoAction;
  }
  return self;
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  // When interrupting `self.postSigninManagerCoordinator` or
  // `self.historySyncPopupCoordinator` below, the signinCompletion is called.
  // This callback is in charge to call `[self
  // runCompletionWithSigninResult: completionIdentity:]`.
  if (self.postSigninManagerCoordinator) {
    DCHECK(!self.addAccountSigninManager);
    [self.postSigninManagerCoordinator interruptWithAction:action
                                                completion:completion];
    return;
  }

  if (self.historySyncPopupCoordinator) {
    DCHECK(!self.addAccountSigninManager);
    [self.historySyncPopupCoordinator interruptWithAction:action
                                               completion:completion];
    return;
  }

  DCHECK(self.addAccountSigninManager);
  [self.addAccountSigninManager interruptWithAction:action
                                         completion:completion];
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  ProfileIOS* profile = self.browser->GetProfile()->GetOriginalProfile();
  _authenticationService = AuthenticationServiceFactory::GetForProfile(profile);
  _syncService = SyncServiceFactory::GetForProfile(profile);
  _accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  _identityManager = IdentityManagerFactory::GetForProfile(profile);
  id<SystemIdentityInteractionManager> identityInteractionManager =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->CreateInteractionManager();
  self.addAccountSigninManager = [[AddAccountSigninManager alloc]
      initWithBaseViewController:self.baseViewController
                     prefService:profile->GetPrefs()
                 identityManager:_identityManager
      identityInteractionManager:identityInteractionManager];
  self.addAccountSigninManager.delegate = self;
  [self.addAccountSigninManager showSigninWithIntent:self.signinIntent];
}

- (void)stop {
  [super stop];
  _accountManagerService = nullptr;
  _identityManager = nullptr;
  _authenticationService = nil;
  _syncService = nil;
  // If one of those 3 DCHECK() fails, -[AddAccountSigninCoordinator
  // runCompletionWithSigninResult] has not been called.
  DCHECK(!self.addAccountSigninManager);
  DCHECK(!self.alertCoordinator);
  DCHECK(!self.postSigninManagerCoordinator);
  DCHECK(!self.historySyncPopupCoordinator);
}

#pragma mark - AddAccountSigninManagerDelegate

- (void)addAccountSigninManagerFailedWithError:(NSError*)error {
  DCHECK(error);
  __weak AddAccountSigninCoordinator* weakSelf = self;
  ProceduralBlock dismissAction = ^{
    [weakSelf.alertCoordinator stop];
    weakSelf.alertCoordinator = nil;
    [weakSelf addAccountSigninManagerFinishedWithSigninResult:
                  SigninCoordinatorResultCanceledByUser
                                                     identity:nil];
  };

  self.alertCoordinator = ErrorCoordinator(
      error, dismissAction, self.baseViewController, self.browser);
  [self.alertCoordinator start];
}

- (void)addAccountSigninManagerFinishedWithSigninResult:
            (SigninCoordinatorResult)signinResult
                                               identity:(id<SystemIdentity>)
                                                            identity {
  if (!self.addAccountSigninManager) {
    // The AddAccountSigninManager callback might be called after the
    // interrupt method. If this is the case, the AddAccountSigninCoordinator
    // is already stopped. This call can be ignored.
    return;
  }
  // Add account is done, we don't need `self.AddAccountSigninManager`
  // anymore.
  self.addAccountSigninManager = nil;
  if (signinResult == SigninCoordinatorResultInterrupted) {
    // Stop the reauth flow.
    [self addAccountDoneWithSigninResult:signinResult identity:nil];
    return;
  }

  // If the signin was successful, but the identity isn't showing up on the
  // device, then it must be an identity that's restricted by policy.
  if (signinResult == SigninCoordinatorResultSuccess) {
    bool identityOnDeviceFound = false;
    if (AreSeparateProfilesForManagedAccountsEnabled()) {
      const std::string gaia = base::SysNSStringToUTF8(identity.gaiaID);
      std::vector<AccountInfo> accountsOnDevice =
          _identityManager->GetAccountsOnDevice();
      for (const AccountInfo& accountInfo : accountsOnDevice) {
        if (accountInfo.gaia == gaia) {
          identityOnDeviceFound = true;
          break;
        }
      }
    } else {
      identityOnDeviceFound = _accountManagerService->IsValidIdentity(identity);
    }
    if (!identityOnDeviceFound) {
      __weak __typeof(self) weakSelf = self;
      // A dispatch is needed to ensure that the alert is displayed after
      // dismissing the signin view.
      dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf presentSignInWithRestrictedAccountAlert];
      });
      return;
    }
  }

  [self continueAddAccountFlowWithSigninResult:signinResult identity:identity];
}

#pragma mark - Private

- (void)stopHistorySyncPopupCoordinator {
  [self.historySyncPopupCoordinator stop];
  self.historySyncPopupCoordinator.delegate = nil;
  self.historySyncPopupCoordinator = nil;
}

- (void)stopPostSigninManagerCoordinator {
  [self.postSigninManagerCoordinator stop];
  self.postSigninManagerCoordinator = nil;
}

// Continues the sign-in workflow according to the sign-in intent
- (void)continueAddAccountFlowWithSigninResult:
            (SigninCoordinatorResult)signinResult
                                      identity:(id<SystemIdentity>)identity {
  // TODO(crbug.com/375605482): Handle the case where the identity is assigned
  // to a different profile. (For kAddAccount this shouldn't matter, and for
  // kPrimaryAccountReauth it should be impossible, but for kResignin it needs
  // to be handled, probably by switching to the other profile and continuing
  // the flow there.)
  switch (self.signinIntent) {
    case AddAccountSigninIntent::kResignin:
      if (signinResult == SigninCoordinatorResultSuccess) {
        [self presentPostSigninManagerCoordinatorWithIdentity:identity];
      } else {
        [self addAccountDoneWithSigninResult:signinResult identity:identity];
      }
      break;
    case AddAccountSigninIntent::kAddAccount:
    case AddAccountSigninIntent::kPrimaryAccountReauth:
      [self addAccountDoneWithSigninResult:signinResult identity:identity];
      break;
  }
}

// Presents an alert when sign in with a restricted account and then continue
// the sign-in workflow.
- (void)presentSignInWithRestrictedAccountAlert {
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:l10n_util::GetNSString(
                                     IDS_IOS_SIGN_IN_INVALID_ACCOUNT_TITLE)
                         message:l10n_util::GetNSString(
                                     IDS_IOS_SIGN_IN_INVALID_ACCOUNT_MESSAGE)];

  __weak __typeof(self) weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                action:^{
                  [weakSelf.alertCoordinator stop];
                  weakSelf.alertCoordinator = nil;
                  [weakSelf continueAddAccountFlowWithSigninResult:
                                SigninCoordinatorResultCanceledByUser
                                                          identity:nil];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
}

- (void)addAccountDone {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(
          self.browser->GetProfile()->GetOriginalProfile());
  // Even if `result` is not "success" for the history opt-in step, the sign-in
  // step did succeed, so pass SigninCoordinatorResultSuccess.
  [self addAccountDoneWithSigninResult:SigninCoordinatorResultSuccess
                              identity:authService->GetPrimaryIdentity(
                                           signin::ConsentLevel::kSignin)];
}

// Runs callback completion on finishing the add account flow.
- (void)addAccountDoneWithSigninResult:(SigninCoordinatorResult)signinResult
                              identity:(id<SystemIdentity>)identity {
  DCHECK(!self.alertCoordinator);
  DCHECK(!self.postSigninManagerCoordinator);
  DCHECK(!self.historySyncPopupCoordinator);
  // `identity` is set, only and only if the sign-in is successful.
  DCHECK(((signinResult == SigninCoordinatorResultSuccess) && identity) ||
         ((signinResult != SigninCoordinatorResultSuccess) && !identity));
  id<SystemIdentity> completionIdentity = identity;
  [self runCompletionWithSigninResult:signinResult
                   completionIdentity:completionIdentity];
}

// Presents the extra screen with `identity` pre-selected.
- (void)presentPostSigninManagerCoordinatorWithIdentity:
    (id<SystemIdentity>)identity {
  // The new UIViewController is presented on top of the currently displayed
  // view controller.
  self.postSigninManagerCoordinator = [SigninCoordinator
      instantSigninCoordinatorWithBaseViewController:self.baseViewController
                                             browser:self.browser
                                            identity:identity
                                         accessPoint:self.accessPoint
                                         promoAction:self.promoAction];

  __weak AddAccountSigninCoordinator* weakSelf = self;
  self.postSigninManagerCoordinator.signinCompletion = ^(
      SigninCoordinatorResult signinResult,
      id<SystemIdentity> signinCompletionIdentity) {
    [weakSelf
        postSigninManagerCoordinatorDoneWithResult:signinResult
                          signinCompletionIdentity:signinCompletionIdentity];
  };
  [self.postSigninManagerCoordinator start];
}

- (void)postSigninManagerCoordinatorDoneWithResult:
            (SigninCoordinatorResult)result
                          signinCompletionIdentity:
                              (id<SystemIdentity>)resultIdentity {
  [self stopPostSigninManagerCoordinator];
  if (result != SigninCoordinatorResultSuccess) {
    [self addAccountDoneWithSigninResult:result identity:resultIdentity];
    return;
  }

  if (history_sync::GetSkipReason(_syncService, _authenticationService,
                                  self.browser->GetProfile()->GetPrefs(),
                                  YES) !=
      history_sync::HistorySyncSkipReason::kNone) {
    [self addAccountDone];
  } else {
    self.historySyncPopupCoordinator = [[HistorySyncPopupCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.browser
                     showUserEmail:NO
                 signOutIfDeclined:NO
                        isOptional:YES
                       accessPoint:self.accessPoint];
    self.historySyncPopupCoordinator.delegate = self;
    [self.historySyncPopupCoordinator start];
  }
}

#pragma mark - HistorySyncPopupCoordinatorDelegate

- (void)historySyncPopupCoordinator:(HistorySyncPopupCoordinator*)coordinator
                didFinishWithResult:(SigninCoordinatorResult)result {
  [self stopHistorySyncPopupCoordinator];
  [self addAccountDone];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"<%@: %p, signinIntent: %d, accessPoint: %d, "
          @"postSigninManagerCoordinator: %p, addAccountSigninManager: "
          @"%p, historySyncPopupCoordinator: %p, alertCoordinator: %p, base "
          @"view controller: %@>",
          self.class.description, self, static_cast<int>(self.signinIntent),
          static_cast<int>(self.accessPoint), self.postSigninManagerCoordinator,
          self.addAccountSigninManager, self.historySyncPopupCoordinator,
          self.alertCoordinator,
          NSStringFromClass(self.baseViewController.class)];
}

@end
