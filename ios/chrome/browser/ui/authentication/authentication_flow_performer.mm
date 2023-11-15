// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"

#import <MaterialComponents/MaterialSnackbar.h>
#import <memory>

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/ios/block_types.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "google_apis/gaia/gaia_urls.h"
#import "ios/chrome/browser/policy/cloud/user_policy_signin_service.h"
#import "ios/chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#import "ios/chrome/browser/policy/cloud/user_policy_switch.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_setup_service.h"
#import "ios/chrome/browser/sync/model/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/settings/import_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"

using signin_ui::CompletionCallback;

namespace {

const int64_t kAuthenticationFlowTimeoutSeconds = 10;
NSString* const kAuthenticationSnackbarCategory =
    @"AuthenticationSnackbarCategory";

}  // namespace

// Content of the managed account confirmation dialog.
@interface ManagedConfirmationDialogContent : NSObject

// Title of the dialog.
@property(nonatomic, readonly, copy) NSString* title;
// Subtitle of the dialog.
@property(nonatomic, readonly, copy) NSString* subtitle;
// Label of the accept button in the dialog.
@property(nonatomic, readonly, copy) NSString* acceptLabel;
// Label of the cancel button in the dialog.
@property(nonatomic, readonly, copy) NSString* cancelLabel;

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                  acceptLabel:(NSString*)acceptLabel
                  cancelLabel:(NSString*)cancelLabel;

@end

@implementation ManagedConfirmationDialogContent

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                  acceptLabel:(NSString*)acceptLabel
                  cancelLabel:(NSString*)cancelLabel {
  if (self = [super init]) {
    _title = title;
    _subtitle = subtitle;
    _acceptLabel = acceptLabel;
    _cancelLabel = cancelLabel;
  }
  return self;
}

@end

@interface AuthenticationFlowPerformer () <ImportDataControllerDelegate,
                                           SettingsNavigationControllerDelegate>
@end

@implementation AuthenticationFlowPerformer {
  __weak id<AuthenticationFlowPerformerDelegate> _delegate;
  AlertCoordinator* _alertCoordinator;
  SettingsNavigationController* _navigationController;
  std::unique_ptr<base::OneShotTimer> _watchdogTimer;
}

- (id<AuthenticationFlowPerformerDelegate>)delegate {
  return _delegate;
}

- (instancetype)initWithDelegate:
    (id<AuthenticationFlowPerformerDelegate>)delegate {
  self = [super init];
  if (self)
    _delegate = delegate;
  return self;
}

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
  if (_navigationController) {
    [_navigationController cleanUpSettings];
    _navigationController = nil;
    switch (action) {
      case SigninCoordinatorInterrupt::UIShutdownNoDismiss:
        if (completion) {
          completion();
        }
        break;
      case SigninCoordinatorInterrupt::DismissWithAnimation:
        if (_delegate) {
          [_delegate dismissPresentingViewControllerAnimated:YES
                                                  completion:completion];
        } else if (completion) {
          completion();
        }
        break;
      case SigninCoordinatorInterrupt::DismissWithoutAnimation:
        if (_delegate) {
          [_delegate dismissPresentingViewControllerAnimated:NO
                                                  completion:completion];
        } else if (completion) {
          completion();
        }
        break;
    }
  } else if (completion) {
    completion();
  }
  _delegate = nil;
  [self stopWatchdogTimer];
}

- (void)fetchManagedStatus:(ChromeBrowserState*)browserState
               forIdentity:(id<SystemIdentity>)identity {
  SystemIdentityManager* systemIdentityManager =
      GetApplicationContext()->GetSystemIdentityManager();
  if (NSString* hostedDomain =
          systemIdentityManager->GetCachedHostedDomainForIdentity(identity)) {
    [_delegate didFetchManagedStatus:hostedDomain];
    return;
  }

  [self startWatchdogTimerForManagedStatus];
  __weak AuthenticationFlowPerformer* weakSelf = self;
  systemIdentityManager->GetHostedDomain(
      identity, base::BindOnce(^(NSString* hostedDomain, NSError* error) {
        [weakSelf handleGetHostedDomain:hostedDomain error:error];
      }));
}

- (void)signInIdentity:(id<SystemIdentity>)identity
         atAccessPoint:(signin_metrics::AccessPoint)accessPoint
      withHostedDomain:(NSString*)hostedDomain
        toBrowserState:(ChromeBrowserState*)browserState {
  AuthenticationServiceFactory::GetForBrowserState(browserState)
      ->SignIn(identity, accessPoint);
}

- (void)signOutBrowserState:(ChromeBrowserState*)browserState {
  __weak __typeof(_delegate) weakDelegate = _delegate;
  AuthenticationServiceFactory::GetForBrowserState(browserState)
      ->SignOut(signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
                /*force_clear_browsing_data=*/false, ^{
                  [weakDelegate didSignOut];
                });
}

- (void)signOutImmediatelyFromBrowserState:(ChromeBrowserState*)browserState {
  AuthenticationServiceFactory::GetForBrowserState(browserState)
      ->SignOut(signin_metrics::ProfileSignout::kAbortSignin,
                /*force_clear_browsing_data=*/false, nil);
}

- (void)promptMergeCaseForIdentity:(id<SystemIdentity>)identity
                           browser:(Browser*)browser
                    viewController:(UIViewController*)viewController {
  DCHECK(browser);
  ChromeBrowserState* browserState = browser->GetBrowserState();
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  // TODO(crbug.com/1462552): After phase 3 migration usage of
  // `lastSyncingEmail` to avoid cross-sync incidents should become obsolete.
  // Delete the usage of ConsentLevel::kSync in this method afterwards.
  // See ConsentLevel::kSync documentation for more details.
  NSString* lastSyncingEmail =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSync)
          .userEmail;
  if (!lastSyncingEmail) {
    // User is not opted in to sync, the current data comes may belong to the
    // previously syncing account (if any).
    lastSyncingEmail =
        base::SysUTF8ToNSString(browserState->GetPrefs()->GetString(
            prefs::kGoogleServicesLastSyncingUsername));
  }

  if (authenticationService->HasPrimaryIdentityManaged(
          signin::ConsentLevel::kSync)) {
    // If the current user is a managed account and sync is enabled, the sign-in
    // needs to wipe the current data. We need to ask confirm from the user.
    AccountInfo primaryAccountInfo = identityManager->FindExtendedAccountInfo(
        identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync));
    DCHECK(!primaryAccountInfo.IsEmpty());
    NSString* hostedDomain =
        base::SysUTF8ToNSString(primaryAccountInfo.hosted_domain);
    [self promptSwitchFromManagedEmail:lastSyncingEmail
                      withHostedDomain:hostedDomain
                               toEmail:identity.userEmail
                        viewController:viewController
                               browser:browser];
    return;
  }
  _navigationController = [SettingsNavigationController
      importDataControllerForBrowser:browser
                            delegate:self
                  importDataDelegate:self
                           fromEmail:lastSyncingEmail
                             toEmail:identity.userEmail];
  [_delegate presentViewController:_navigationController
                          animated:YES
                        completion:nil];
}

- (void)clearDataFromBrowser:(Browser*)browser
              commandHandler:(id<BrowsingDataCommands>)handler {
  DCHECK(browser);
  ChromeBrowserState* browserState = browser->GetBrowserState();
  // The user needs to be signed out when clearing the data to avoid deleting
  // data on server side too.
  CHECK(!AuthenticationServiceFactory::GetForBrowserState(browserState)
             ->HasPrimaryIdentity(signin::ConsentLevel::kSignin));

  // Workaround for crbug.com/1003578
  //
  // During the Chrome sign-in flow, if the user chooses to clear the cookies
  // when switching their primary account, then the following actions take
  // place (in the following order):
  //   1. All cookies are cleared.
  //   2. The user is signed in to Chrome (aka the primary account set)
  //   2.a. All requests to Gaia page will include Mirror header.
  //   2.b. Account reconcilor will rebuild the Gaia cookies having the Chrome
  //      primary account as the Gaia default web account.
  //
  // The Gaia sign-in webpage monitors changes to its cookies and reloads the
  // page whenever they change. Reloading the webpage while the cookies are
  // cleared and just before they are rebuilt seems to confuse WKWebView that
  // ends up with a nil URL, which in turns translates in about:blank URL shown
  // in the Omnibox.
  //
  // This CL works around this issue by waiting for 1 second between steps 1
  // and 2 above to allow the WKWebView to initiate the reload after the
  // cookies are cleared.
  WebStateList* webStateList = browser->GetWebStateList();
  web::WebState* activeWebState = webStateList->GetActiveWebState();
  bool activeWebStateHasGaiaOrigin =
      activeWebState &&
      (activeWebState->GetVisibleURL().DeprecatedGetOriginAsURL() ==
       GaiaUrls::GetInstance()->gaia_url());
  int64_t dispatchDelaySecs = activeWebStateHasGaiaOrigin ? 1 : 0;
  __weak __typeof(_delegate) weakDelegate = _delegate;
  [handler removeBrowsingDataForBrowserState:browserState
                                  timePeriod:browsing_data::TimePeriod::ALL_TIME
                                  removeMask:BrowsingDataRemoveMask::REMOVE_ALL
                             completionBlock:^{
                               dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                                            dispatchDelaySecs *
                                                                NSEC_PER_SEC),
                                              dispatch_get_main_queue(), ^{
                                                [weakDelegate didClearData];
                                              });
                             }];
}

- (BOOL)shouldHandleMergeCaseForIdentity:(id<SystemIdentity>)identity
                       browserStatePrefs:(PrefService*)prefs {
  const std::string lastSignedInGaiaId =
      prefs->GetString(prefs::kGoogleServicesLastSyncingGaiaId);
  if (!lastSignedInGaiaId.empty()) {
    // Merge case exists if the id of the previously signed in account is
    // different from the one of the account being signed in.
    return lastSignedInGaiaId != base::SysNSStringToUTF8(identity.gaiaID);
  }

  // kGoogleServicesLastSyncingGaiaId pref might not have been populated yet,
  // check the old kGoogleServicesLastSyncingUsername pref.
  const std::string currentSignedInEmail =
      base::SysNSStringToUTF8(identity.userEmail);
  const std::string lastSignedInEmail =
      prefs->GetString(prefs::kGoogleServicesLastSyncingUsername);
  return !lastSignedInEmail.empty() &&
         !gaia::AreEmailsSame(currentSignedInEmail, lastSignedInEmail);
}

// Retuns the ManagedConfirmationDialogContent that corresponds to the
// provided `hostedDomain`, `syncConsent`, and the activation state of User
// Policy.
- (ManagedConfirmationDialogContent*)
    managedConfirmationDialogContentForHostedDomain:(NSString*)hostedDomain
                                        syncConsent:(BOOL)syncConsent {
  if (!policy::IsAnyUserPolicyFeatureEnabled()) {
    // Show the legacy managed confirmation dialog if User Policy is disabled.
    return [[ManagedConfirmationDialogContent alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_MANAGED_SIGNIN_TITLE)
             subtitle:l10n_util::GetNSStringF(
                          IDS_IOS_MANAGED_SIGNIN_SUBTITLE,
                          base::SysNSStringToUTF16(hostedDomain))
          acceptLabel:l10n_util::GetNSString(
                          IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON)
          cancelLabel:l10n_util::GetNSString(IDS_CANCEL)];
  } else if (syncConsent) {
    // Show the first version of the managed confirmation dialog for User Policy
    // if User Policy is enabled and there is Sync consent.
    return [[ManagedConfirmationDialogContent alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_MANAGED_SYNC_TITLE)
             subtitle:l10n_util::GetNSStringF(
                          IDS_IOS_MANAGED_SYNC_WITH_USER_POLICY_SUBTITLE,
                          base::SysNSStringToUTF16(hostedDomain))
          acceptLabel:l10n_util::GetNSString(
                          IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON)
          cancelLabel:l10n_util::GetNSString(IDS_CANCEL)];
  } else {
    // Show the release version of the managed confirmation dialog for User
    // Policy if User Policy is enabled and there is no Sync consent.
    return [[ManagedConfirmationDialogContent alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_MANAGED_SIGNIN_TITLE)
             subtitle:l10n_util::GetNSStringF(
                          IDS_IOS_MANAGED_SIGNIN_WITH_USER_POLICY_SUBTITLE,
                          base::SysNSStringToUTF16(hostedDomain))
          acceptLabel:
              l10n_util::GetNSString(
                  IDS_IOS_MANAGED_SIGNIN_WITH_USER_POLICY_CONTINUE_BUTTON_LABEL)
          cancelLabel:l10n_util::GetNSString(IDS_CANCEL)];
  }
}

- (void)showManagedConfirmationForHostedDomain:(NSString*)hostedDomain
                                viewController:(UIViewController*)viewController
                                       browser:(Browser*)browser
                                   syncConsent:(BOOL)syncConsent {
  DCHECK(!_alertCoordinator);

  ManagedConfirmationDialogContent* content =
      [self managedConfirmationDialogContentForHostedDomain:hostedDomain
                                                syncConsent:syncConsent];

  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:viewController
                                                   browser:browser
                                                     title:content.title
                                                   message:content.subtitle];

  __weak AuthenticationFlowPerformer* weakSelf = self;
  __weak AlertCoordinator* weakAlert = _alertCoordinator;

  ProceduralBlock acceptBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;

    // TODO(crbug.com/1326767): Nullify the browser object in the
    // AlertCoordinator when the coordinator is stopped to avoid using the
    // browser object at that moment, in which case the browser object may have
    // been deleted before the callback block is called. This is to avoid
    // potential bad memory accesses.
    Browser* alertedBrowser = weakAlert.browser;
    if (alertedBrowser) {
      PrefService* prefService = alertedBrowser->GetBrowserState()->GetPrefs();
      // TODO(crbug.com/1325115): Remove this line once we determined that the
      // notification isn't needed anymore.
      [strongSelf updateUserPolicyNotificationStatusIfNeeded:prefService];
    }

    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate] didAcceptManagedConfirmation];
  };
  ProceduralBlock cancelBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate] didCancelManagedConfirmation];
  };

  [_alertCoordinator addItemWithTitle:content.cancelLabel
                               action:cancelBlock
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator addItemWithTitle:content.acceptLabel
                               action:acceptBlock
                                style:UIAlertActionStyleDefault];
  _alertCoordinator.noInteractionAction = cancelBlock;
  [_alertCoordinator start];
}

- (void)showSnackbarWithSignInIdentity:(id<SystemIdentity>)identity
                               browser:(Browser*)browser {
  DCHECK(browser);
  base::WeakPtr<Browser> weakBrowser = browser->AsWeakPtr();
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = ^{
    if (!weakBrowser.get()) {
      return;
    }
    base::RecordAction(
        base::UserMetricsAction("Mobile.Signin.SnackbarUndoTapped"));
    ChromeBrowserState* browserState =
        weakBrowser->GetBrowserState()->GetOriginalChromeBrowserState();
    AuthenticationService* authService =
        AuthenticationServiceFactory::GetForBrowserState(browserState);
    if (authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
      authService->SignOut(
          signin_metrics::ProfileSignout::kUserTappedUndoRightAfterSignIn,
          /*force_clear_browsing_data=*/false, nil);
    }
  };
  action.title = l10n_util::GetNSString(IDS_IOS_SIGNIN_SNACKBAR_UNDO);
  action.accessibilityIdentifier = kSigninSnackbarUndo;
  NSString* messageText =
      l10n_util::GetNSStringF(IDS_IOS_SIGNIN_SNACKBAR_SIGNED_IN_AS,
                              base::SysNSStringToUTF16(identity.userEmail));
  MDCSnackbarMessage* message =
      [MDCSnackbarMessage messageWithText:messageText];
  message.action = action;
  message.category = kAuthenticationSnackbarCategory;

  id<SnackbarCommands> handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SnackbarCommands);
  CHECK(handler);
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  [handler showSnackbarMessage:message];
}

- (void)showAuthenticationError:(NSError*)error
                 withCompletion:(ProceduralBlock)callback
                 viewController:(UIViewController*)viewController
                        browser:(Browser*)browser {
  DCHECK(!_alertCoordinator);

  _alertCoordinator = ErrorCoordinatorNoItem(error, viewController, browser);

  __weak AuthenticationFlowPerformer* weakSelf = self;
  __weak AlertCoordinator* weakAlert = _alertCoordinator;
  ProceduralBlock dismissAction = ^{
    [weakSelf alertControllerDidDisappear:weakAlert];
    if (callback)
      callback();
  };

  NSString* okButtonLabel = l10n_util::GetNSString(IDS_OK);
  [_alertCoordinator addItemWithTitle:okButtonLabel
                               action:dismissAction
                                style:UIAlertActionStyleDefault];

  [_alertCoordinator start];
}

- (void)registerUserPolicy:(ChromeBrowserState*)browserState
               forIdentity:(id<SystemIdentity>)identity {
  // Should only fetch user policies when the feature is enabled.
  DCHECK(policy::IsAnyUserPolicyFeatureEnabled());

  std::string userEmail = base::SysNSStringToUTF8(identity.userEmail);
  CoreAccountId accountID =
      IdentityManagerFactory::GetForBrowserState(browserState)
          ->PickAccountIdForAccount(base::SysNSStringToUTF8(identity.gaiaID),
                                    userEmail);

  policy::UserPolicySigninService* userPolicyService =
      policy::UserPolicySigninServiceFactory::GetForBrowserState(browserState);

  __weak __typeof(self) weakSelf = self;

  [self startWatchdogTimerForUserPolicyRegistration];
  userPolicyService->RegisterForPolicyWithAccountId(
      userEmail, accountID,
      base::BindOnce(^(const std::string& dmToken, const std::string& clientID,
                       const std::vector<std::string>& userAffiliationIDs) {
        if (![self stopWatchdogTimer]) {
          // Watchdog timer has already fired, don't notify the delegate.
          return;
        }
        NSMutableArray<NSString*>* userAffiliationIDsNSArray =
            [[NSMutableArray alloc] init];
        for (const auto& userAffiliationID : userAffiliationIDs) {
          [userAffiliationIDsNSArray
              addObject:base::SysUTF8ToNSString(userAffiliationID)];
        }
        [weakSelf.delegate
            didRegisterForUserPolicyWithDMToken:base::SysUTF8ToNSString(dmToken)
                                       clientID:base::SysUTF8ToNSString(
                                                    clientID)
                             userAffiliationIDs:userAffiliationIDsNSArray];
      }));
}

- (void)fetchUserPolicy:(ChromeBrowserState*)browserState
            withDmToken:(NSString*)dmToken
               clientID:(NSString*)clientID
     userAffiliationIDs:(NSArray<NSString*>*)userAffiliationIDs
               identity:(id<SystemIdentity>)identity {
  // Should only fetch user policies when the feature is enabled.
  DCHECK(policy::IsAnyUserPolicyFeatureEnabled());

  // Need a `dmToken` and a `clientID` to fetch user policies.
  DCHECK([dmToken length] > 0);
  DCHECK([clientID length] > 0);

  policy::UserPolicySigninService* policyService =
      policy::UserPolicySigninServiceFactory::GetForBrowserState(browserState);
  const std::string userEmail = base::SysNSStringToUTF8(identity.userEmail);

  AccountId accountID =
      AccountId::FromUserEmailGaiaId(gaia::CanonicalizeEmail(userEmail),
                                     base::SysNSStringToUTF8(identity.gaiaID));

  __weak __typeof(self) weakSelf = self;

  std::vector<std::string> userAffiliationIDsVector;
  for (NSString* userAffiliationID in userAffiliationIDs) {
    userAffiliationIDsVector.push_back(
        base::SysNSStringToUTF8(userAffiliationID));
  }

  [self startWatchdogTimerForUserPolicyFetch];
  policyService->FetchPolicyForSignedInUser(
      accountID, base::SysNSStringToUTF8(dmToken),
      base::SysNSStringToUTF8(clientID), userAffiliationIDsVector,
      browserState->GetSharedURLLoaderFactory(),
      base::BindOnce(^(bool success) {
        if (![self stopWatchdogTimer]) {
          // Watchdog timer has already fired, don't notify the delegate.
          return;
        }
        [weakSelf.delegate didFetchUserPolicyWithSuccess:success];
      }));
}

#pragma mark - ImportDataControllerDelegate

- (void)didChooseClearDataPolicy:(ImportDataTableViewController*)controller
                 shouldClearData:(ShouldClearData)shouldClearData {
  DCHECK_NE(SHOULD_CLEAR_DATA_USER_CHOICE, shouldClearData);
  if (shouldClearData == SHOULD_CLEAR_DATA_CLEAR_DATA) {
    base::RecordAction(
        base::UserMetricsAction("Signin_ImportDataPrompt_DontImport"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Signin_ImportDataPrompt_ImportData"));
  }

  __weak AuthenticationFlowPerformer* weakSelf = self;
  ProceduralBlock block = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    strongSelf->_navigationController = nil;
    [[strongSelf delegate] didChooseClearDataPolicy:shouldClearData];
  };
  [_navigationController cleanUpSettings];
  [_delegate dismissPresentingViewControllerAnimated:YES completion:block];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  base::RecordAction(base::UserMetricsAction("Signin_ImportDataPrompt_Cancel"));

  __weak AuthenticationFlowPerformer* weakSelf = self;
  ProceduralBlock block = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    strongSelf->_navigationController = nil;
    [[strongSelf delegate] didChooseCancel];
  };
  [_navigationController cleanUpSettings];
  [_delegate dismissPresentingViewControllerAnimated:YES completion:block];
}

- (void)settingsWasDismissed {
  base::RecordAction(base::UserMetricsAction("Signin_ImportDataPrompt_Cancel"));
  [self.delegate didChooseCancel];
  [_navigationController cleanUpSettings];
  _navigationController = nil;
}

#pragma mark - Private

- (void)updateUserPolicyNotificationStatusIfNeeded:(PrefService*)prefService {
  if (!policy::IsAnyUserPolicyFeatureEnabled()) {
    // Don't set the notification pref if the User Policy feature isn't
    // enabled.
    return;
  }

  prefService->SetBoolean(policy::policy_prefs::kUserPolicyNotificationWasShown,
                          true);
}

- (void)handleGetHostedDomain:(NSString*)hostedDomain error:(NSError*)error {
  if (![self stopWatchdogTimer]) {
    // Watchdog timer has already fired, don't notify the delegate.
    return;
  }
  if (error) {
    [_delegate didFailFetchManagedStatus:error];
    return;
  }
  [_delegate didFetchManagedStatus:hostedDomain];
}

// Starts a Watchdog Timer that calls `timeoutBlock` on time out.
- (void)startWatchdogTimerWithTimeoutBlock:(ProceduralBlock)timeoutBlock {
  DCHECK(!_watchdogTimer);
  _watchdogTimer.reset(new base::OneShotTimer());
  _watchdogTimer->Start(FROM_HERE,
                        base::Seconds(kAuthenticationFlowTimeoutSeconds),
                        base::BindOnce(timeoutBlock));
}

// Starts the watchdog timer with a timeout of
// `kAuthenticationFlowTimeoutSeconds` for the fetching managed status
// operation. It will notify `_delegate` of the failure unless
// `stopWatchdogTimer` is called before it times out.
- (void)startWatchdogTimerForManagedStatus {
  __weak AuthenticationFlowPerformer* weakSelf = self;
  ProceduralBlock timeoutBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf stopWatchdogTimer];
    NSError* error = [NSError errorWithDomain:kAuthenticationErrorDomain
                                         code:TIMED_OUT_FETCH_POLICY
                                     userInfo:nil];
    [strongSelf->_delegate didFailFetchManagedStatus:error];
  };
  [self startWatchdogTimerWithTimeoutBlock:timeoutBlock];
}

// Starts a Watchdog Timer that ends the user policy registration on time out.
- (void)startWatchdogTimerForUserPolicyRegistration {
  __weak AuthenticationFlowPerformer* weakSelf = self;
  ProceduralBlock timeoutBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf stopWatchdogTimer];
    [strongSelf.delegate didRegisterForUserPolicyWithDMToken:@""
                                                    clientID:@""
                                          userAffiliationIDs:@[]];
  };
  [self startWatchdogTimerWithTimeoutBlock:timeoutBlock];
}

// Starts a Watchdog Timer that ends the user policy fetch on time out.
- (void)startWatchdogTimerForUserPolicyFetch {
  __weak AuthenticationFlowPerformer* weakSelf = self;
  ProceduralBlock timeoutBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf stopWatchdogTimer];
    [strongSelf->_delegate didFetchUserPolicyWithSuccess:NO];
  };
  [self startWatchdogTimerWithTimeoutBlock:timeoutBlock];
}

// Stops the watchdog timer, and doesn't call the `timeoutDelegateSelector`.
// Returns whether the watchdog was actually running.
- (BOOL)stopWatchdogTimer {
  if (_watchdogTimer) {
    _watchdogTimer->Stop();
    _watchdogTimer.reset();
    return YES;
  }
  return NO;
}

- (void)promptSwitchFromManagedEmail:(NSString*)managedEmail
                    withHostedDomain:(NSString*)hostedDomain
                             toEmail:(NSString*)toEmail
                      viewController:(UIViewController*)viewController
                             browser:(Browser*)browser {
  DCHECK(!_alertCoordinator);
  NSString* title = l10n_util::GetNSString(IDS_IOS_MANAGED_SWITCH_TITLE);
  NSString* subtitle = l10n_util::GetNSStringF(
      IDS_IOS_MANAGED_SWITCH_SUBTITLE, base::SysNSStringToUTF16(managedEmail),
      base::SysNSStringToUTF16(toEmail),
      base::SysNSStringToUTF16(hostedDomain));
  NSString* acceptLabel =
      l10n_util::GetNSString(IDS_IOS_MANAGED_SWITCH_ACCEPT_BUTTON);
  NSString* cancelLabel = l10n_util::GetNSString(IDS_CANCEL);

  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:viewController
                                                   browser:browser
                                                     title:title
                                                   message:subtitle];

  __weak AuthenticationFlowPerformer* weakSelf = self;
  __weak AlertCoordinator* weakAlert = _alertCoordinator;
  ProceduralBlock acceptBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate]
        didChooseClearDataPolicy:SHOULD_CLEAR_DATA_CLEAR_DATA];
  };
  ProceduralBlock cancelBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate] didChooseCancel];
  };

  [_alertCoordinator addItemWithTitle:cancelLabel
                               action:cancelBlock
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator addItemWithTitle:acceptLabel
                               action:acceptBlock
                                style:UIAlertActionStyleDefault];
  _alertCoordinator.noInteractionAction = cancelBlock;
  [_alertCoordinator start];
}

// Callback for when the alert is dismissed.
- (void)alertControllerDidDisappear:(AlertCoordinator*)alertCoordinator {
  if (_alertCoordinator != alertCoordinator) {
    // Do not reset the `_alertCoordinator` if it has changed. This typically
    // happens when the user taps on any of the actions on "Clear Data Before
    // Syncing?" dialog, as the sign-in confirmation dialog is created before
    // the "Clear Data Before Syncing?" dialog is dismissed.
    return;
  }
  _alertCoordinator = nil;
}

@end
