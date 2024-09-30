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
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "google_apis/gaia/gaia_urls.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service_factory.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_switch.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"

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
  if ((self = [super init])) {
    _title = title;
    _subtitle = subtitle;
    _acceptLabel = acceptLabel;
    _cancelLabel = cancelLabel;
  }
  return self;
}

@end

@implementation AuthenticationFlowPerformer {
  __weak id<AuthenticationFlowPerformerDelegate> _delegate;
  // This code uses three variables for alert coordinators in order to clarify
  // crash reports related to crbug.com/1482623
  // TODO(crbug.com/40072272): The 2 alert coordinator variables can be merged
  // into one alert coordinator once the bug is fixed.
  // Dialog for the managed confirmation dialog.
  AlertCoordinator* _managedConfirmationAlertCoordinator;
  // Dialog to display an error.
  AlertCoordinator* _errorAlertCoordinator;
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
  [_managedConfirmationAlertCoordinator stop];
  _managedConfirmationAlertCoordinator = nil;
  [_errorAlertCoordinator stop];
  _errorAlertCoordinator = nil;
  if (completion) {
    completion();
  }
  _delegate = nil;
  [self stopWatchdogTimer];
}

- (void)fetchManagedStatus:(ProfileIOS*)profile
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
             toProfile:(ProfileIOS*)profile {
  AuthenticationServiceFactory::GetForProfile(profile)->SignIn(identity,
                                                               accessPoint);
}

- (void)signOutProfile:(ProfileIOS*)profile {
  __weak __typeof(_delegate) weakDelegate = _delegate;
  AuthenticationServiceFactory::GetForProfile(profile)->SignOut(
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
      /*force_clear_browsing_data=*/false, ^{
        [weakDelegate didSignOut];
      });
}

- (void)signOutImmediatelyFromProfile:(ProfileIOS*)profile {
  AuthenticationServiceFactory::GetForProfile(profile)->SignOut(
      signin_metrics::ProfileSignout::kAbortSignin,
      /*force_clear_browsing_data=*/false, nil);
}

// Retuns the ManagedConfirmationDialogContent that corresponds to the
// provided `hostedDomain`, and the activation state of User Policy.
- (ManagedConfirmationDialogContent*)
    managedConfirmationDialogContentForHostedDomain:(NSString*)hostedDomain {
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
                                       browser:(Browser*)browser {
  DCHECK(!_managedConfirmationAlertCoordinator);
  DCHECK(!_errorAlertCoordinator);

  ManagedConfirmationDialogContent* content =
      [self managedConfirmationDialogContentForHostedDomain:hostedDomain];

  base::RecordAction(
      base::UserMetricsAction("Signin_AuthenticationFlowPerformer_"
                              "ManagedConfirmationDialog_Presented"));
  _managedConfirmationAlertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:viewController
                                                   browser:browser
                                                     title:content.title
                                                   message:content.subtitle];

  __weak AuthenticationFlowPerformer* weakSelf = self;
  __weak AlertCoordinator* weakAlert = _managedConfirmationAlertCoordinator;

  ProceduralBlock acceptBlock = ^{
    base::RecordAction(
        base::UserMetricsAction("Signin_AuthenticationFlowPerformer_"
                                "ManagedConfirmationDialog_Confirmed"));

    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;

    // TODO(crbug.com/40225944): Nullify the browser object in the
    // AlertCoordinator when the coordinator is stopped to avoid using the
    // browser object at that moment, in which case the browser object may have
    // been deleted before the callback block is called. This is to avoid
    // potential bad memory accesses.
    Browser* alertedBrowser = weakAlert.browser;
    if (alertedBrowser) {
      PrefService* prefService = alertedBrowser->GetProfile()->GetPrefs();
      // TODO(crbug.com/40225352): Remove this line once we determined that the
      // notification isn't needed anymore.
      [strongSelf updateUserPolicyNotificationStatusIfNeeded:prefService];
    }

    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate] didAcceptManagedConfirmation];
  };
  ProceduralBlock cancelBlock = ^{
    base::RecordAction(
        base::UserMetricsAction("Signin_AuthenticationFlowPerformer_"
                                "ManagedConfirmationDialog_Canceled"));
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate] didCancelManagedConfirmation];
  };

  [_managedConfirmationAlertCoordinator
      addItemWithTitle:content.cancelLabel
                action:cancelBlock
                 style:UIAlertActionStyleCancel];
  [_managedConfirmationAlertCoordinator
      addItemWithTitle:content.acceptLabel
                action:acceptBlock
                 style:UIAlertActionStyleDefault];
  _managedConfirmationAlertCoordinator.noInteractionAction = cancelBlock;
  [_managedConfirmationAlertCoordinator start];
}

- (void)completePostSignInActions:(PostSignInActionSet)postSignInActions
                     withIdentity:(id<SystemIdentity>)identity
                          browser:(Browser*)browser {
  DCHECK(browser);
  base::WeakPtr<Browser> weakBrowser = browser->AsWeakPtr();
  ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);

  // Signing in from bookmarks and reading list enables the corresponding
  // type.
  BOOL bookmarksToggleEnabledWithSigninFlow = NO;
  BOOL readingListToggleEnabledWithSigninFlow = NO;
  if (postSignInActions.Has(
          PostSignInAction::kEnableUserSelectableTypeBookmarks) &&
      !syncService->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kBookmarks)) {
    syncService->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kBookmarks, true);
    bookmarksToggleEnabledWithSigninFlow = YES;
  } else if (postSignInActions.Has(
                 PostSignInAction::kEnableUserSelectableTypeReadingList) &&
             !syncService->GetUserSettings()->GetSelectedTypes().Has(
                 syncer::UserSelectableType::kReadingList)) {
    syncService->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kReadingList, true);
    readingListToggleEnabledWithSigninFlow = YES;
  }

  if (!postSignInActions.Has(PostSignInAction::kShowSnackbar)) {
    return;
  }

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = ^{
    if (!weakBrowser.get()) {
      return;
    }
    base::RecordAction(
        base::UserMetricsAction("Mobile.Signin.SnackbarUndoTapped"));
    AuthenticationService* authService =
        AuthenticationServiceFactory::GetForProfile(profile);
    if (authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
      // Signing in from bookmarks and reading list enables the corresponding
      // type. The undo button should handle that before signing out.
      if (bookmarksToggleEnabledWithSigninFlow) {
        syncService->GetUserSettings()->SetSelectedType(
            syncer::UserSelectableType::kBookmarks, false);
      } else if (readingListToggleEnabledWithSigninFlow) {
        syncService->GetUserSettings()->SetSelectedType(
            syncer::UserSelectableType::kReadingList, false);
      }
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
  MDCSnackbarMessage* message = CreateSnackbarMessage(messageText);
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
  DCHECK(!_managedConfirmationAlertCoordinator);
  DCHECK(!_errorAlertCoordinator);

  base::RecordAction(base::UserMetricsAction(
      "Signin_AuthenticationFlowPerformer_ErrorDialog_Presented"));
  _errorAlertCoordinator =
      ErrorCoordinatorNoItem(error, viewController, browser);

  __weak AuthenticationFlowPerformer* weakSelf = self;
  __weak AlertCoordinator* weakAlert = _errorAlertCoordinator;
  ProceduralBlock dismissAction = ^{
    base::RecordAction(base::UserMetricsAction(
        "Signin_AuthenticationFlowPerformer_ErrorDialog_Confirmed"));
    [weakSelf alertControllerDidDisappear:weakAlert];
    if (callback) {
      callback();
    }
  };

  NSString* okButtonLabel = l10n_util::GetNSString(IDS_OK);
  [_errorAlertCoordinator addItemWithTitle:okButtonLabel
                                    action:dismissAction
                                     style:UIAlertActionStyleDefault];

  [_errorAlertCoordinator start];
}

- (void)registerUserPolicy:(ProfileIOS*)profile
               forIdentity:(id<SystemIdentity>)identity {
  // Should only fetch user policies when the feature is enabled.
  DCHECK(policy::IsAnyUserPolicyFeatureEnabled());

  std::string userEmail = base::SysNSStringToUTF8(identity.userEmail);
  CoreAccountId accountID =
      IdentityManagerFactory::GetForProfile(profile)->PickAccountIdForAccount(
          base::SysNSStringToUTF8(identity.gaiaID), userEmail);

  policy::UserPolicySigninService* userPolicyService =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile);

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

- (void)fetchUserPolicy:(ProfileIOS*)profile
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
      policy::UserPolicySigninServiceFactory::GetForProfile(profile);
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
      profile->GetSharedURLLoaderFactory(), base::BindOnce(^(bool success) {
        if (![self stopWatchdogTimer]) {
          // Watchdog timer has already fired, don't notify the delegate.
          return;
        }
        [weakSelf.delegate didFetchUserPolicyWithSuccess:success];
      }));
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

// Callback for when the alert is dismissed.
- (void)alertControllerDidDisappear:(AlertCoordinator*)alertCoordinator {
  if (_managedConfirmationAlertCoordinator == alertCoordinator) {
    _managedConfirmationAlertCoordinator = nil;
  } else if (_errorAlertCoordinator == alertCoordinator) {
    _errorAlertCoordinator = nil;
  }
  // TODO(crbug.com/40072272): This code needs to be simpler and clearer.
  // At least NOTREACHED should be added here.
}

@end
