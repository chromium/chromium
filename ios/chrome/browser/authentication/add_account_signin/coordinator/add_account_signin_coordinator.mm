// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/add_account_signin/coordinator/add_account_signin_coordinator.h"

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/add_account_signin/coordinator/add_account_signin_manager.h"
#import "ios/chrome/browser/authentication/add_account_signin/coordinator/add_account_signin_mediator.h"
#import "ios/chrome/browser/authentication/add_account_signin/coordinator/add_account_signin_mediator_delegate.h"
#import "ios/chrome/browser/authentication/history_sync/coordinator/history_sync_coordinator.h"
#import "ios/chrome/browser/authentication/history_sync/coordinator/history_sync_popup_coordinator.h"
#import "ios/chrome/browser/authentication/history_sync/model/history_sync_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator+protected.h"
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
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@interface AddAccountSigninCoordinator () <AddAccountSigninManagerDelegate,
                                           AddAccountSigninMediatorDelegate,
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
  ChangeProfileContinuationProvider _continuationProvider;
  AddAccountSigninMediator* _mediator;
  // Email to pre-fill.
  NSString* _prefilledEmail;
  BOOL _stopped;
}

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              contextStyle:(SigninContextStyle)contextStyle
                               accessPoint:(AccessPoint)accessPoint
                               promoAction:(PromoAction)promoAction
                              signinIntent:(AddAccountSigninIntent)signinIntent
                            prefilledEmail:(NSString*)email
                      continuationProvider:
                          (const ChangeProfileContinuationProvider&)
                              continuationProvider {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                              contextStyle:contextStyle
                               accessPoint:accessPoint];
  if (self) {
    CHECK(continuationProvider);
    CHECK(viewController, base::NotFatalUntil::M140);
    _continuationProvider = continuationProvider;
    _signinIntent = signinIntent;
    _promoAction = promoAction;
    _prefilledEmail = [email copy];
  }
  return self;
}

- (void)dealloc {
  CHECK(!_continuationProvider, base::NotFatalUntil::M145);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  ProfileIOS* profile = self.profile->GetOriginalProfile();
  _authenticationService = AuthenticationServiceFactory::GetForProfile(profile);
  _mediator = [[AddAccountSigninMediator alloc]
      initWithAuthenticationService:AuthenticationServiceFactory::GetForProfile(
                                        profile)];
  _mediator.delegate = self;
  switch (_signinIntent) {
    case AddAccountSigninIntent::kAddAccount:
      // It is possible to have a primary identity when adding a secondary
      // identity. It is possible to have no primary identity when doing a first
      // sign-in.
      break;
    case AddAccountSigninIntent::kPrimaryAccountReauth:
      // The user wants to reauth their primary account.
      CHECK(_authenticationService->HasPrimaryIdentity(
                signin::ConsentLevel::kSignin),
            base::NotFatalUntil::M143);
      break;
    case AddAccountSigninIntent::kResignin:
      // The user wants to add back their primary account.
      CHECK(!_authenticationService->HasPrimaryIdentity(
                signin::ConsentLevel::kSignin),
            base::NotFatalUntil::M143);
      break;
  }
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
      identityInteractionManager:identityInteractionManager
                  prefilledEmail:_prefilledEmail];
  self.addAccountSigninManager.delegate = self;
  // Note that, up to iOS 18, the view may disappear if the user turn off their
  // screen, without informing the delegate, due to a bug in UIKit. See
  // crbug.com/395959814.
  [self.addAccountSigninManager showSigninWithIntent:self.signinIntent];
}

#pragma mark - AnimatedCoordinator

- (void)stopAnimated:(BOOL)animated {
  // The coordinator should not be stopped twice. Indeed, after the first stop,
  // the coordinator will be released, and this will cause the second stop to be
  // executed on a zombie object. See crbug.com/442589294.
  CHECK(!_stopped, base::NotFatalUntil::M145);
  _stopped = YES;
  [super stopAnimated:animated];
  [self stopPostSigninManagerCoordinatorAnimated:animated];
  [self interruptAddAccountSigninManager:animated];
  [self stopAlertCoordinator];
  [self stopHistorySyncPopupCoordinator];

  [_mediator disconnect];
  _mediator.delegate = nil;
  _mediator = nil;
  _accountManagerService = nullptr;
  _identityManager = nullptr;
  _authenticationService = nil;
  _continuationProvider.Reset();
  _syncService = nil;
}

#pragma mark - AddAccountSigninManagerDelegate

- (void)addAccountSigninManagerFinishedWithResult:
            (SigninAddAccountToDeviceResult)result
                                         identity:(id<SystemIdentity>)identity
                                            error:(NSError*)error {
  if (!self.addAccountSigninManager) {
    // The AddAccountSigninManager callback might be called after the
    // interrupt method. If this is the case, the AddAccountSigninCoordinator
    // is already stopped. This call can be ignored.
    return;
  }
  // Add account is done, we don't need `self.AddAccountSigninManager`
  // anymore.
  self.addAccountSigninManager.delegate = nil;
  self.addAccountSigninManager = nil;

  switch (result) {
    case SigninAddAccountToDeviceResult::kInterrupted:
      // Stop the reauth flow.
      [self addAccountDoneWithSigninResult:SigninCoordinatorResultInterrupted
                                  identity:nil];
      return;
    case SigninAddAccountToDeviceResult::kCancelledByUser:
      [self addAccountDoneWithSigninResult:SigninCoordinatorResultCanceledByUser
                                  identity:nil];
      return;
    case SigninAddAccountToDeviceResult::kError: {
      DCHECK(error);
      __weak AddAccountSigninCoordinator* weakSelf = self;
      ProceduralBlock dismissAction = ^{
        [weakSelf stopAlertCoordinator];
        [weakSelf
            addAccountDoneWithSigninResult:SigninCoordinatorResultCanceledByUser
                                  identity:nil];
      };

      self.alertCoordinator = ErrorCoordinator(
          error, dismissAction, self.baseViewController, self.browser);
      [self.alertCoordinator start];
      return;
    }
    case SigninAddAccountToDeviceResult::kSuccess: {
      // If the signin was successful, but the identity isn't showing up on the
      // device, then it must be an identity that's restricted by policy.
      bool identityOnDeviceFound = false;
      const GaiaId gaia(identity.gaiaId);
      std::vector<AccountInfo> accountsOnDevice =
          _identityManager->GetAccountsOnDevice();
      for (const AccountInfo& accountInfo : accountsOnDevice) {
        if (accountInfo.gaia == gaia) {
          identityOnDeviceFound = true;
          break;
        }
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
      [self
          continueAddAccountFlowWithSigninResult:SigninCoordinatorResultSuccess
                                        identity:identity];
    }
  }
}

#pragma mark - BuggyAuthenticationViewOwner

- (BOOL)viewWillPersist {
  if (@available(iOS 26, *)) {
    // The authentication view doesn’t disappear silently on iOS 26.
    return YES;
  }
  // Once the authentication is done, the manager is set to nil and the view
  // can’t have disappeared.
  return self.addAccountSigninManager == nil;
}

#pragma mark - Private

// Returns whether the coordinator is started and not stopped.
- (BOOL)isStarted {
  return !_continuationProvider.is_null();
}

- (void)stopAlertCoordinator {
  // To avoid reentry, `noInteractionAction` is set to nil.
  self.alertCoordinator.noInteractionAction = nil;
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

- (void)interruptAddAccountSigninManager:(BOOL)animated {
  [self.addAccountSigninManager interruptAnimated:animated];
  self.addAccountSigninManager.delegate = nil;
  self.addAccountSigninManager = nil;
}

- (void)stopHistorySyncPopupCoordinator {
  [self.historySyncPopupCoordinator stop];
  self.historySyncPopupCoordinator.delegate = nil;
  self.historySyncPopupCoordinator = nil;
}

- (void)stopPostSigninManagerCoordinatorAnimated:(BOOL)animated {
  [self.postSigninManagerCoordinator stopAnimated:animated];
  self.postSigninManagerCoordinator = nil;
}

// Continues the sign-in workflow according to the sign-in intent
- (void)continueAddAccountFlowWithSigninResult:
            (SigninCoordinatorResult)signinResult
                                      identity:(id<SystemIdentity>)identity {
  // TODO(crbug.com/400902218): Handle the case where the identity is assigned
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
                  [weakSelf stopAlertCoordinator];
                  [weakSelf continueAddAccountFlowWithSigninResult:
                                SigninCoordinatorResultCanceledByUser
                                                          identity:nil];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
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
  if (_stopped) {
    // The delegate is already dealing with stopping `self`.
    return;
  }
  [self runCompletionWithSigninResult:signinResult completionIdentity:identity];
}

// Presents the extra screen with `identity` pre-selected.
- (void)presentPostSigninManagerCoordinatorWithIdentity:
    (id<SystemIdentity>)identity {
  if (_identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // Due to asynchronicity, it can occurs that the users already got
    // signed-in.
    [self runCompletionWithSigninResult:SigninCoordinatorResultInterrupted
                     completionIdentity:nil];
    return;
  }
  CHECK([self isStarted], base::NotFatalUntil::M144);
  // The new UIViewController is presented on top of the currently displayed
  // view controller.
  self.postSigninManagerCoordinator = [SigninCoordinator
      instantSigninCoordinatorWithBaseViewController:self.baseViewController
                                             browser:self.browser
                                            identity:identity
                                        contextStyle:self.contextStyle
                                         accessPoint:self.accessPoint
                                         promoAction:self.promoAction
                                continuationProvider:_continuationProvider];

  __weak AddAccountSigninCoordinator* weakSelf = self;
  self.postSigninManagerCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult signinResult,
        id<SystemIdentity> signinCompletionIdentity) {
        [weakSelf postSigninManagerCoordinatorDoneWithCoordinator:coordinator
                                                           result:signinResult
                                         signinCompletionIdentity:
                                             signinCompletionIdentity];
      };
  [self.postSigninManagerCoordinator start];
}

- (void)postSigninManagerCoordinatorDoneWithCoordinator:
            (SigninCoordinator*)coordinator
                                                 result:
                                                     (SigninCoordinatorResult)
                                                         result
                               signinCompletionIdentity:
                                   (id<SystemIdentity>)resultIdentity {
  CHECK_EQ(self.postSigninManagerCoordinator, coordinator,
           base::NotFatalUntil::M151);
  [self stopPostSigninManagerCoordinatorAnimated:NO];
  if (result != SigninCoordinatorResultSuccess) {
    [self addAccountDoneWithSigninResult:result identity:resultIdentity];
    return;
  }

  if (history_sync::GetSkipReason(_syncService, _authenticationService,
                                  self.profile->GetPrefs(), YES) !=
      history_sync::HistorySyncSkipReason::kNone) {
    [self addAccountDoneWithSigninResult:result identity:resultIdentity];
    return;
  }
  CHECK([self isStarted], base::NotFatalUntil::M144);
  self.historySyncPopupCoordinator = [[HistorySyncPopupCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                   showUserEmail:NO
               signOutIfDeclined:NO
                      isOptional:YES
                    contextStyle:self.contextStyle
                     accessPoint:self.accessPoint];
  self.historySyncPopupCoordinator.delegate = self;
  [self.historySyncPopupCoordinator start];
}

#pragma mark - HistorySyncPopupCoordinatorDelegate

- (void)historySyncPopupCoordinator:(HistorySyncPopupCoordinator*)coordinator
                didFinishWithResult:(HistorySyncResult)result {
  [self stopHistorySyncPopupCoordinator];
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(
          self.profile->GetOriginalProfile());
  id<SystemIdentity> primaryIdentity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  SigninCoordinatorResult signinResult;
  switch (result) {
    case HistorySyncResult::kUserCanceled:
    case HistorySyncResult::kSuccess:
    case HistorySyncResult::kSkipped:
      signinResult = SigninCoordinatorResultSuccess;
      CHECK(primaryIdentity, base::NotFatalUntil::M145);
      break;
    case HistorySyncResult::kPrimaryIdentityRemoved:
      signinResult = SigninCoordinatorResultInterrupted;
      CHECK(!primaryIdentity, base::NotFatalUntil::M145);
      break;
  }
  [self addAccountDoneWithSigninResult:signinResult identity:primaryIdentity];
}

#pragma mark - AddAccountSigninMediatorDelegate

- (void)mediatorWantsToBeStopped:(AddAccountSigninMediator*)mediator {
  CHECK_EQ(_mediator, mediator, base::NotFatalUntil::M144);
  if (_stopped) {
    // Stop is already ongoing.
    return;
  }
  [self runCompletionWithSigninResult:SigninCoordinatorResultInterrupted
                   completionIdentity:nil];
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
