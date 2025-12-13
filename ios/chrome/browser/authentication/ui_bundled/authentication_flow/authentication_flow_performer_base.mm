// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_base.h"

#import <memory>
#import <optional>

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
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
#import "google_apis/gaia/gaia_id.h"
#import "google_apis/gaia/gaia_urls.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/browser/authentication/history_sync/model/history_sync_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_base+protected.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_base_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/bubble/model/utils.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service_factory.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
const int64_t kAuthenticationFlowTimeoutSeconds = 10;

// The change profile continuation for the authentication flow.
void AuthenticationFlowContinuationImpl(
    id<AuthenticationFlowPerformerBaseDelegate> delegate,
    SceneState* scene_state,
    base::OnceClosure closure) {
  CHECK(delegate);
  [delegate
      didSwitchToProfileWithNewProfileBrowser:scene_state
                                                  .browserProviderInterface
                                                  .mainBrowserProvider.browser
                                   completion:std::move(closure)];
}

// Handler for the signout action from a snackbar. Will `clear_selected_type`
// if it is not std::nullopt.
void HandleSignoutForSnackbar(
    base::WeakPtr<Browser> weak_browser,
    std::optional<syncer::UserSelectableType> clear_selected_type) {
  Browser* browser = weak_browser.get();
  if (!browser) {
    return;
  }
  // The regular browser should be used to execute the signout.
  CHECK_EQ(browser->type(), Browser::Type::kRegular, base::NotFatalUntil::M145);

  base::RecordAction(
      base::UserMetricsAction("Mobile.Signin.SnackbarUndoTapped"));

  ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (!auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    return;
  }

  if (clear_selected_type.has_value()) {
    SyncServiceFactory::GetForProfile(profile)
        ->GetUserSettings()
        ->SetSelectedType(clear_selected_type.value(), false);
  }

  // To complete the signout request, it should be guranteed to complete the
  // request using a non-incognito browser.
  Browser* mainBrowser =
      browser->type() == Browser::Type::kIncognito
          ? browser->GetSceneState()
                .browserProviderInterface.mainBrowserProvider.browser
          : browser;
  signin::ProfileSignoutRequest(
      signin_metrics::ProfileSignout::kUserTappedUndoRightAfterSignIn)
      .Run(mainBrowser);
}

void MaybeShowHistorySyncScreenAfterProfileSwitch(
    Browser* browser,
    signin_metrics::AccessPoint access_point,
    id<SystemIdentity> identity,
    BOOL showSnackbar) {
  ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  if (history_sync::GetSkipReason(syncService, authenticationService,
                                  profile->GetPrefs(), /*isOptional=*/NO) !=
      history_sync::HistorySyncSkipReason::kNone) {
    if (showSnackbar) {
      TriggerAccountSwitchSnackbarWithIdentity(identity, browser);
    }
    return;
  }

  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kHistorySync
               identity:nil
            accessPoint:access_point
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
             completion:nil];
  command.showSnackbar = showSnackbar;
  command.optionalHistorySync = YES;

  UIViewController* view_controller =
      browser->GetSceneState().window.rootViewController;
  while (view_controller.presentedViewController) {
    view_controller = view_controller.presentedViewController;
  }

  [browser->GetSceneState().controller showSignin:command
                               baseViewController:view_controller];
}

void CompletePostSignInActionsContinuationImpl(
    PostSignInActionSet post_signin_actions,
    id<SystemIdentity> identity,
    signin_metrics::AccessPoint access_point,
    SceneState* scene_state,
    base::OnceClosure closure) {
  Browser* browser =
      scene_state.browserProviderInterface.mainBrowserProvider.browser;
  CompletePostSignInActions(post_signin_actions, identity, browser,
                            access_point);
  std::move(closure).Run();
}

ChangeProfileContinuation CompletePostSigninActionsContinuation(
    PostSignInActionSet post_signin_actions,
    id<SystemIdentity> identity,
    signin_metrics::AccessPoint access_point) {
  return base::BindOnce(&CompletePostSignInActionsContinuationImpl,
                        post_signin_actions, identity, access_point);
}

}  // namespace

void CompletePostSignInActions(PostSignInActionSet post_signin_actions,
                               id<SystemIdentity> identity,
                               Browser* browser,
                               signin_metrics::AccessPoint access_point) {
  CHECK(browser, base::NotFatalUntil::M145);
  // Sign-in related work should be done on regular browser.
  CHECK_EQ(browser->type(), Browser::Type::kRegular, base::NotFatalUntil::M145);
  ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);

  // Signing in from bookmarks and reading list enables the corresponding
  // type.
  std::optional<syncer::UserSelectableType> clear_selectable_type;
  if (post_signin_actions.Has(
          PostSignInAction::kEnableUserSelectableTypeBookmarks) &&
      !sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kBookmarks)) {
    sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kBookmarks, true);
    clear_selectable_type = syncer::UserSelectableType::kBookmarks;
  } else if (post_signin_actions.Has(
                 PostSignInAction::kEnableUserSelectableTypeReadingList) &&
             !sync_service->GetUserSettings()->GetSelectedTypes().Has(
                 syncer::UserSelectableType::kReadingList)) {
    sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kReadingList, true);
    clear_selectable_type = syncer::UserSelectableType::kReadingList;
  }

  if (post_signin_actions.Has(
          PostSignInAction::kShowHistorySyncScreenAfterProfileSwitch)) {
    BOOL showSnackbar = post_signin_actions.Has(
        PostSignInAction::kShowIdentityConfirmationSnackbar);
    MaybeShowHistorySyncScreenAfterProfileSwitch(browser, access_point,
                                                 identity, showSnackbar);
    return;
  }

  if (post_signin_actions.Has(
          PostSignInAction::kShowIdentityConfirmationSnackbar)) {
    TriggerAccountSwitchSnackbarWithIdentity(identity, browser);
    return;
  }

  if (!post_signin_actions.Has(PostSignInAction::kShowSnackbar)) {
    return;
  }

  SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
  action.handler = base::CallbackToBlock(base::BindOnce(
      &HandleSignoutForSnackbar, browser->AsWeakPtr(), clear_selectable_type));

  action.title = l10n_util::GetNSString(IDS_IOS_SIGNIN_SNACKBAR_UNDO);
  NSString* messageText =
      l10n_util::GetNSStringF(IDS_IOS_SIGNIN_SNACKBAR_SIGNED_IN_AS,
                              base::SysNSStringToUTF16(identity.userEmail));
  SnackbarMessage* message =
      [[SnackbarMessage alloc] initWithTitle:messageText];
  message.action = action;

  id<SnackbarCommands> handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SnackbarCommands);
  CHECK(handler);
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  [handler showSnackbarMessage:message];
}

@implementation AuthenticationFlowPerformerBase {
  __weak id<AuthenticationFlowPerformerBaseDelegate> _delegate;
  // Dialog to display an error.
  AlertCoordinator* _errorAlertCoordinator;
  std::unique_ptr<base::OneShotTimer> _watchdogTimer;
  id<ChangeProfileCommands> _changeProfileHandler;
}

- (instancetype)
        initWithDelegate:(id<AuthenticationFlowPerformerBaseDelegate>)delegate
    changeProfileHandler:(id<ChangeProfileCommands>)changeProfileHandler {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _changeProfileHandler = changeProfileHandler;
  }
  return self;
}

- (void)switchToProfileWithName:(const std::string&)profileName
                     sceneState:(SceneState*)sceneState
                         reason:(ChangeProfileReason)reason
      changeProfileContinuation:
          (ChangeProfileContinuation)requestHelperContinuation
              postSignInActions:(PostSignInActionSet)postSignInActions
                   withIdentity:(id<SystemIdentity>)identity
                    accessPoint:(signin_metrics::AccessPoint)accessPoint {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());
  ChangeProfileContinuation postSignInContinuation =
      CompletePostSigninActionsContinuation(postSignInActions, identity,
                                            accessPoint);
  ChangeProfileContinuation authenticationFlowContinuation =
      [self authenticationFlowContinuation];
  ChangeProfileContinuation fullContinuation = ChainChangeProfileContinuations(
      std::move(authenticationFlowContinuation),
      ChainChangeProfileContinuations(std::move(requestHelperContinuation),
                                      std::move(postSignInContinuation)));
  [_changeProfileHandler changeProfile:profileName
                              forScene:sceneState
                                reason:reason
                          continuation:std::move(fullContinuation)];
}

- (void)showAuthenticationError:(NSError*)error
                 withCompletion:(ProceduralBlock)callback
                 viewController:(UIViewController*)viewController
                        browser:(Browser*)browser {
  CHECK(browser, base::NotFatalUntil::M150);
  // Sign-in related work should be done on regular browser.
  CHECK_EQ(browser->type(), Browser::Type::kRegular, base::NotFatalUntil::M145);
  [self checkNoDialog];

  base::RecordAction(base::UserMetricsAction(
      "Signin_AuthenticationFlowPerformer_ErrorDialog_Presented"));
  _errorAlertCoordinator =
      ErrorCoordinatorNoItem(error, viewController, browser);

  __weak AuthenticationFlowPerformerBase* weakSelf = self;
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

#pragma mark - Private

// The change profile continuation for the authentication flow.
- (ChangeProfileContinuation)authenticationFlowContinuation {
  return base::BindOnce(&AuthenticationFlowContinuationImpl, _delegate);
}

#pragma mark - Private

// Callback for when the alert is dismissed.
- (void)alertControllerDidDisappear:(ChromeCoordinator*)coordinator {
  [self checkNoDialog];
  [_errorAlertCoordinator stop];
  _errorAlertCoordinator = nil;
}

@end

@implementation AuthenticationFlowPerformerBase (Protected)

// Starts a Watchdog Timer that calls `timeoutBlock` on time out.
- (void)startWatchdogTimerWithTimeoutBlock:(ProceduralBlock)timeoutBlock {
  DCHECK(!_watchdogTimer);
  _watchdogTimer.reset(new base::OneShotTimer());
  _watchdogTimer->Start(FROM_HERE,
                        base::Seconds(kAuthenticationFlowTimeoutSeconds),
                        base::BindOnce(timeoutBlock));
}

- (BOOL)stopWatchdogTimer {
  if (_watchdogTimer) {
    _watchdogTimer->Stop();
    _watchdogTimer.reset();
    return YES;
  }
  return NO;
}

- (void)checkNoDialog {
  // Dialogs are not displayed directly by the base class.
}

@end
