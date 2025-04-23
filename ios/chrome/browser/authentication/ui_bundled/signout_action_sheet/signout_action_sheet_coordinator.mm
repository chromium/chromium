// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signout_action_sheet/signout_action_sheet_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/check.h"
#import "base/format_macros.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/enterprise_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using signin_metrics::SignoutDataLossAlertReason;

// Wrapper around the SignoutActionSheetCoordinator completion taking care
// of properly handling cancellation and profile change.
@interface SignoutActionSheetCompletionWrapper : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCompletion:(signin_ui::SignoutCompletionCallback)block
    NS_DESIGNATED_INITIALIZER;

// Record whether a change of profile is expected (if true, then calls to
// -coordinatorStopped are ignored).
@property(nonatomic, assign) BOOL willChangeProfile;

// Called when the sign-out operation completes, invoke the completion
// with success unless the coordinator was stopped and the sign-out
// operation did not change the profile.
- (void)signoutCompleteForScene:(SceneState*)sceneState;

// Called when the coordinator is stopped. Will invoke the completion
// with failure unless the sign-out operation requires changing the
// profile (as this will destroy the UI and thus stop the coordinator).
- (void)coordinatorStoppedForScene:(SceneState*)sceneState;

@end

@implementation SignoutActionSheetCompletionWrapper {
  // Completion callback.
  signin_ui::SignoutCompletionCallback _completion;
}

- (instancetype)initWithCompletion:(signin_ui::SignoutCompletionCallback)block {
  if ((self = [super init])) {
    _completion = block;
    DCHECK(_completion);
  }
  return self;
}

- (void)signoutCompleteForScene:(SceneState*)sceneState {
  [self invokeCompletion:YES sceneState:sceneState];
}

- (void)coordinatorStoppedForScene:(SceneState*)sceneState {
  if (!_willChangeProfile) {
    [self invokeCompletion:NO sceneState:sceneState];
  }
}

- (void)invokeCompletion:(BOOL)success sceneState:(SceneState*)sceneState {
  if (signin_ui::SignoutCompletionCallback completion = _completion) {
    _completion = nil;
    completion(success, sceneState);
  }
}

@end

@interface SignoutActionSheetCoordinator () {
  // YES if the coordinator asked its delegate to block the user interaction.
  // This boolean makes sure the user interaction is allowed when `stop` is
  // called.
  BOOL _userActionBlocked;
  // YES if the coordinator has been stopped.
  BOOL _stopped;
  // Rectangle for the popover alert.
  CGRect _rect;
  // View for the popovert alert.
  __weak UIView* _view;
  // Source of the sign-out action. For histogram if the sign-out occurs.
  signin_metrics::ProfileSignout _signoutSourceMetric;
  // Show the snackbar above the snackbar.
  BOOL _forceSnackbarOverToolbar;
  // Signin and syncing state.
  SignedInUserState _signedInUserState;
  // Wrapper around the completion callback.
  SignoutActionSheetCompletionWrapper* _completionWrapper;
}

// Service for managing identity authentication.
@property(nonatomic, assign, readonly)
    AuthenticationService* authenticationService;
// Action sheet to display sign-out actions.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;
// YES if sign-in is forced by enterprise policy.
@property(nonatomic, assign, readonly) BOOL isForceSigninEnabled;

@end

@implementation SignoutActionSheetCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                          rect:(CGRect)rect
                          view:(UIView*)view
      forceSnackbarOverToolbar:(BOOL)forceSnackbarOverToolbar
                    withSource:(signin_metrics::ProfileSignout)source
                    completion:(signin_ui::SignoutCompletionCallback)block {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _rect = rect;
    _view = view;
    _signoutSourceMetric = source;
    _forceSnackbarOverToolbar = forceSnackbarOverToolbar;
    _completionWrapper =
        [[SignoutActionSheetCompletionWrapper alloc] initWithCompletion:block];
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.authenticationService->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  PrefService* profilePrefService = self.profile->GetPrefs();
  _signedInUserState = GetSignedInUserState(
      self.authenticationService, self.identityManager, profilePrefService);
  if (ForceLeavingPrimaryAccountConfirmationDialog(
          _signedInUserState, self.profile->GetProfileName())) {
    [self startActionSheetCoordinatorForSignout];
  } else {
    [self checkForUnsyncedDataAndSignOut];
  }
}

- (void)stop {
  if (_userActionBlocked) {
    [self allowUserInteraction];
  }
  [self dismissActionSheetCoordinator];
  _stopped = YES;
  SignoutActionSheetCompletionWrapper* completionWrapper = _completionWrapper;
  _completionWrapper = nil;
  [completionWrapper coordinatorStoppedForScene:nil];
  // `self` may be deallocated after `coordinatorStoppedForScene`.
}

- (void)dealloc {
  DCHECK(!_userActionBlocked);
  DCHECK(_stopped);
  DCHECK(!self.actionSheetCoordinator);
}

#pragma mark - ActionSheetCoordinator properties

- (NSString*)title {
  return self.actionSheetCoordinator.title;
}

- (NSString*)message {
  return self.actionSheetCoordinator.message;
}

#pragma mark - Browser-based properties

- (AuthenticationService*)authenticationService {
  return AuthenticationServiceFactory::GetForProfile(self.profile);
}

- (signin::IdentityManager*)identityManager {
  return IdentityManagerFactory::GetForProfile(self.profile);
}

#pragma mark - Properties

- (BOOL)isForceSigninEnabled {
  return self.authenticationService->GetServiceStatus() ==
         AuthenticationService::ServiceStatus::SigninForcedByPolicy;
}

#pragma mark - Private

// Calls the delegate to prevent user actions, and updates `_userActionBlocked`.
- (void)preventUserInteraction {
  DCHECK(!_userActionBlocked);
  _userActionBlocked = YES;
  [self.delegate signoutActionSheetCoordinatorPreventUserInteraction:self];
}

// Calls the delegate to allow user actions, and updates `_userActionBlocked`.
- (void)allowUserInteraction {
  DCHECK(_userActionBlocked);
  _userActionBlocked = NO;
  [self.delegate signoutActionSheetCoordinatorAllowUserInteraction:self];
}

// Wraps -allowUserInteraction and does nothing if -stop has been called.
- (void)allowUserInteractionIfNotStopped {
  if (!_stopped) {
    [self allowUserInteraction];
  }
}

// Fetches for unsynced data, and the sign-out continued after (with unsynced
// data dialog if needed, and then sign-out).
- (void)checkForUnsyncedDataAndSignOut {
  [self preventUserInteraction];

  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile(self.profile);
  __weak __typeof(self) weakSelf = self;
  auto callback = base::BindOnce(^(syncer::DataTypeSet set) {
    [weakSelf continueSignOutWithUnsyncedDataTypeSet:set];
  });
  signin::FetchUnsyncedDataForSignOutOrProfileSwitching(syncService,
                                                        std::move(callback));
}

// Displays the sign-out confirmation dialog if `set` contains an "interesting"
// data type, otherwise the sign-out is triggered without dialog.
- (void)continueSignOutWithUnsyncedDataTypeSet:(syncer::DataTypeSet)set {
  [self allowUserInteraction];
  if (!set.empty()) {
    for (syncer::DataType type : set) {
      base::UmaHistogramEnumeration("Sync.UnsyncedDataOnSignout2",
                                    syncer::DataTypeHistogramValue(type));
    }
    [self startActionSheetCoordinatorForSignout];
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Signin_Signout_ConfirmationRequestNotPresented"));
    [self handleSignOut];
  }
}

- (void)dismissActionSheetCoordinator {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
}

// Starts the signout action sheet for the current user state.
- (void)startActionSheetCoordinatorForSignout {
  __weak __typeof(self) weakSelf = self;
  self.actionSheetCoordinator = GetLeavingPrimaryAccountConfirmationDialog(
      self.baseViewController, self.browser, _view, _rect, _signedInUserState,
      /*account_profile_switch=*/false, ^(BOOL continueFlow) {
        [weakSelf signoutConfirmationWithContinue:continueFlow];
      });
  base::RecordAction(
      base::UserMetricsAction("Signin_Signout_ConfirmationRequestPresented"));
  [self.actionSheetCoordinator start];
}

- (void)signoutConfirmationWithContinue:(BOOL)continueSignout {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
  if (continueSignout) {
    [self handleSignOut];
    [self dismissActionSheetCoordinator];
  } else {
    SceneState* sceneState = nil;
    if (Browser* browser = self.browser) {
      sceneState = browser->GetSceneState();
    }
    [_completionWrapper coordinatorStoppedForScene:sceneState];
    _completionWrapper = nil;

    [self dismissActionSheetCoordinator];
  }
}

// Signs the user out of the primary account and clears the data from their
// device if account is managed.
- (void)handleSignOut {
  Browser* browser = self.browser;
  if (!browser) {
    return;
  }

  if (!self.authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    SceneState* sceneState = browser->GetSceneState();
    [_completionWrapper signoutCompleteForScene:sceneState];
    _completionWrapper = nil;
    return;
  }

  [self preventUserInteraction];
  // Prepare the signout snackbar before account switching.
  // The snackbar message might be nil if the snackbar is not needed.
  MDCSnackbarMessage* snackbarMessage = [self signoutSnackbarMessage];

  // Strongly retain completionWrapper in the blocks to ensure that the
  // completion callback will be invoked even if the UI is destroyed
  // (e.g. when the sign-out operation needs to change profile).
  SignoutActionSheetCompletionWrapper* completionWrapper = _completionWrapper;

  __weak __typeof(self) weakSelf = self;
  signin::ProfileSignoutRequest(_signoutSourceMetric)
      .SetSnackbarMessage(snackbarMessage, _forceSnackbarOverToolbar)
      .SetPrepareCallback(base::BindOnce(^(bool will_change_profile) {
        completionWrapper.willChangeProfile = will_change_profile;
      }))
      .SetCompletionCallback(base::BindOnce(^(SceneState* scene_state) {
        [weakSelf allowUserInteractionIfNotStopped];
        [completionWrapper signoutCompleteForScene:scene_state];
      }))
      .Run(browser);
}

// Returns snackbar if needed.
- (MDCSnackbarMessage*)signoutSnackbarMessage {
  if (self.isForceSigninEnabled) {
    // Snackbar should be skipped since force sign-in dialog will be shown right
    // after.
    return nil;
  }
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile(self.profile);
  int message_id =
      syncService->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
              HasManagedSyncDataType(syncService)
          ? IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_SNACKBAR_MESSAGE_ENTERPRISE
          : IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_SNACKBAR_MESSAGE;
  MDCSnackbarMessage* message =
      CreateSnackbarMessage(l10n_util::GetNSString(message_id));
  return message;
}

@end
