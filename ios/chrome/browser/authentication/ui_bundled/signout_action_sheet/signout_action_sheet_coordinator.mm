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
  signin_metrics::ProfileSignout _signout_source_metric;
  // Show the snackbar above the snackbar.
  BOOL _forceSnackbarOverToolbar;
  // Signin and syncing state.
  SignedInUserState _signedInUserState;
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

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      rect:(CGRect)rect
                                      view:(UIView*)view
                  forceSnackbarOverToolbar:(BOOL)forceSnackbarOverToolbar
                                withSource:(signin_metrics::ProfileSignout)
                                               signout_source_metric {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _rect = rect;
    _view = view;
    _signout_source_metric = signout_source_metric;
    _forceSnackbarOverToolbar = forceSnackbarOverToolbar;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.signoutCompletion);
  DCHECK(self.authenticationService->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  PrefService* profilePrefService = self.browser->GetProfile()->GetPrefs();
  _signedInUserState = GetSignedInUserState(
      self.authenticationService, self.identityManager, profilePrefService);
  if (ForceLeavingPrimaryAccountConfirmationDialog(_signedInUserState)) {
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
  [self callCompletionBlock:NO];
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
  return AuthenticationServiceFactory::GetForProfile(
      self.browser->GetProfile());
}

- (signin::IdentityManager*)identityManager {
  return IdentityManagerFactory::GetForProfile(self.browser->GetProfile());
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

// Fetches for unsynced data, and the sign-out continued after (with unsynced
// data dialog if needed, and then sign-out).
- (void)checkForUnsyncedDataAndSignOut {
  [self preventUserInteraction];

  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile(self.browser->GetProfile());
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
    [self callCompletionBlock:NO];
    [self dismissActionSheetCoordinator];
  }
}

// Signs the user out of the primary account and clears the data from their
// device if account is managed.
- (void)handleSignOut {
  if (!self.browser) {
    return;
  }

  if (!self.authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    [self callCompletionBlock:YES];
    return;
  }
  [self preventUserInteraction];
  // Prepare the signout snackbar before account switching.
  // The snackbar message might be nil if the snackbar is not needed.
  MDCSnackbarMessage* snackbarMessage = [self signoutSnackbarMessage];

  __weak __typeof(self) weakSelf = self;
  signin::MultiProfileSignOut(self.browser, _signout_source_metric,
                              _forceSnackbarOverToolbar, snackbarMessage, ^{
                                [weakSelf signOutDidFinish];
                              });
}

// Called when the sign-out is done.
- (void)signOutDidFinish {
  if (_stopped) {
    // The coordinator has been stopped. The UI has been unblocked, and the
    // owner doesn't expect the completion call anymore.
    return;
  }
  [self allowUserInteraction];
  [self callCompletionBlock:YES];
}

// Returns snackbar if needed.
- (MDCSnackbarMessage*)signoutSnackbarMessage {
  if (self.isForceSigninEnabled) {
    // Snackbar should be skipped since force sign-in dialog will be shown right
    // after.
    return nil;
  }
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile(self.browser->GetProfile());
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

// Calls `self.signoutCompletion` if available, and sets it to `null` before the
// call.
- (void)callCompletionBlock:(BOOL)signedOut {
  if (!self.signoutCompletion) {
    return;
  }
  signin_ui::SignoutCompletionCallback completion = self.signoutCompletion;
  self.signoutCompletion = nil;
  completion(signedOut);
}

@end
