// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signout_action_sheet/signout_action_sheet_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/check.h"
#import "base/format_macros.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/utf_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/enterprise_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Enum to describe all 5 cases for a user being signed-in. This enum is used
// internaly by SignoutActionSheetCoordinator().
typedef NS_ENUM(NSUInteger, SignedInUserState) {
  // sign-in with UNO. The sign-out needs to ask confirmation to sign out only
  // if there are unsaved data. When signed out, a snackbar needs to be
  // diplayed.
  SignedInUserStateWithNotSyncingAndReplaceSyncWithSignin,
  // Sign-in with a managed account and sync is turned on.
  SignedInUserStateWithManagedAccountAndSyncing,
  // Sign-in with a managed account and sync is turned off.
  SignedInUserStateWithManagedAccountAndNotSyncing,
  // Sign-in with a regular account and sync is turned on.
  SignedInUserStateWithNonManagedAccountAndSyncing,
  // Sign-in with a regular account and sync is turned off.
  SignedInUserStateWithNoneManagedAccountAndNotSyncing,
  // Sign-in with a requirement to give more contextual information when the
  // forced sign-in policy is enabled.
  SignedInUserStateWithForcedSigninInfoRequired
};

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
}

// Service for managing identity authentication.
@property(nonatomic, assign, readonly)
    AuthenticationService* authenticationService;
// Action sheet to display sign-out actions.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;
// YES if the user has confirmed that they want to signout.
@property(nonatomic, assign) BOOL confirmSignOut;
// YES if sign-in is forced by enterprise policy.
@property(nonatomic, assign, readonly) BOOL isForceSigninEnabled;

@end

@implementation SignoutActionSheetCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      rect:(CGRect)rect
                                      view:(UIView*)view
                                withSource:(signin_metrics::ProfileSignout)
                                               signout_source_metric {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _rect = rect;
    _view = view;
    _signout_source_metric = signout_source_metric;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.completion);
  DCHECK(self.authenticationService->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  switch (self.signedInUserState) {
    case SignedInUserStateWithNotSyncingAndReplaceSyncWithSignin:
      [self checkForUnsyncedDataAndSignOut];
      break;
    case SignedInUserStateWithManagedAccountAndSyncing:
    case SignedInUserStateWithManagedAccountAndNotSyncing:
    case SignedInUserStateWithNonManagedAccountAndSyncing:
    case SignedInUserStateWithNoneManagedAccountAndNotSyncing:
    case SignedInUserStateWithForcedSigninInfoRequired:
      [self startActionSheetCoordinatorForSignout];
      break;
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
  return AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

// Returns the user's sign-in and syncing state.
- (SignedInUserState)signedInUserState {
  DCHECK(self.browser);
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());
  // TODO(crbug.com/1462552): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  if (!self.authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSync) &&
      base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return SignedInUserStateWithNotSyncingAndReplaceSyncWithSignin;
  }
  BOOL syncEnabled =
      syncService->GetUserSettings()->IsInitialSyncFeatureSetupComplete();

  // Need a first step to show logout contextual information about the forced
  // sign-in policy. Only return this state when sync is enabled because it is
  // already shown for sync disabled.
  if (self.isForceSigninEnabled && syncEnabled && !self.confirmSignOut) {
    return SignedInUserStateWithForcedSigninInfoRequired;
  }

  if (self.authenticationService->HasPrimaryIdentityManaged(
          signin::ConsentLevel::kSignin)) {
    return syncEnabled ? SignedInUserStateWithManagedAccountAndSyncing
                       : SignedInUserStateWithManagedAccountAndNotSyncing;
  }
  return syncEnabled ? SignedInUserStateWithNonManagedAccountAndSyncing
                     : SignedInUserStateWithNoneManagedAccountAndNotSyncing;
}

// Returns the title associated to the given user sign-in state or nil if no
// title is defined for the state.
- (NSString*)actionSheetCoordinatorTitle {
  DCHECK(self.browser);
  NSString* title = nil;
  switch (self.signedInUserState) {
    case SignedInUserStateWithNotSyncingAndReplaceSyncWithSignin:
      // This dialog is triggered only if there is unsync data.
      title = l10n_util::GetNSString(
          IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_AND_DELETE_TITLE);
      break;
    case SignedInUserStateWithManagedAccountAndSyncing: {
      std::u16string hostedDomain = HostedDomainForPrimaryAccount(self.browser);
      title = l10n_util::GetNSStringF(
          IDS_IOS_SIGNOUT_DIALOG_TITLE_WITH_SYNCING_MANAGED_ACCOUNT,
          hostedDomain);
      break;
    }
    case SignedInUserStateWithNonManagedAccountAndSyncing: {
      title = l10n_util::GetNSString(
          IDS_IOS_SIGNOUT_DIALOG_TITLE_WITH_SYNCING_ACCOUNT);
      break;
    }
    case SignedInUserStateWithForcedSigninInfoRequired:
    case SignedInUserStateWithManagedAccountAndNotSyncing:
    case SignedInUserStateWithNoneManagedAccountAndNotSyncing: {
      if (self.isForceSigninEnabled) {
        title = l10n_util::GetNSString(
            IDS_IOS_ENTERPRISE_FORCED_SIGNIN_SIGNOUT_DIALOG_TITLE);
      } else if (self.showUnavailableFeatureDialogHeader) {
        title = l10n_util::GetNSString(
            IDS_IOS_SIGNOUT_DIALOG_TITLE_WITHOUT_SYNCING_ACCOUNT);
      }
      break;
    }
  }

  return title;
}

// Returns the message associated to the given user sign-in state or nil if no
// message is defined for the state.
- (NSString*)actionSheetCoordinatorMessage {
  switch (self.signedInUserState) {
    case SignedInUserStateWithNotSyncingAndReplaceSyncWithSignin:
      // This dialog is triggered only if there is unsync data.
      return l10n_util::GetNSString(
          IDS_IOS_SIGNOUT_DIALOG_MESSAGE_WITH_NOT_SAVED_DATA);
    case SignedInUserStateWithForcedSigninInfoRequired:
    case SignedInUserStateWithNoneManagedAccountAndNotSyncing:
    case SignedInUserStateWithManagedAccountAndNotSyncing: {
      if (self.isForceSigninEnabled) {
        return l10n_util::GetNSString(IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE);
      }
      return nil;
    }
    case SignedInUserStateWithManagedAccountAndSyncing:
    case SignedInUserStateWithNonManagedAccountAndSyncing: {
      return nil;
    }
  }
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
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());
  __weak __typeof(self) weakSelf = self;
  auto callback = base::BindOnce(^(syncer::ModelTypeSet set) {
    [weakSelf continueSignOutWithUnsyncedDataModelTypeSet:set];
  });
  syncService->GetTypesWithUnsyncedData(std::move(callback));
}

// Displays the sign-out confirmation dialog if `set` contains an "interesting"
// data type, otherwise the sign-out is triggered without dialog.
- (void)continueSignOutWithUnsyncedDataModelTypeSet:(syncer::ModelTypeSet)set {
  [self allowUserInteraction];
  set.RetainAll({syncer::BOOKMARKS, syncer::READING_LIST, syncer::PASSWORDS,
                 syncer::CONTACT_INFO});
  if (!set.Empty()) {
    for (syncer::ModelType type : set) {
      base::UmaHistogramEnumeration("Sync.UnsyncedDataOnSignout",
                                    syncer::ModelTypeForHistograms(type));
    }
    [self startActionSheetCoordinatorForSignout];
  } else {
    [self handleSignOutWithForceClearData:NO];
  }
}

- (void)dismissActionSheetCoordinator {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
}

// Starts the signout action sheet for the current user state.
- (void)startActionSheetCoordinatorForSignout {
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:self.actionSheetCoordinatorTitle
                         message:self.actionSheetCoordinatorMessage
                            rect:_rect
                            view:_view];

  __weak SignoutActionSheetCoordinator* weakSelf = self;
  switch (self.signedInUserState) {
    case SignedInUserStateWithNotSyncingAndReplaceSyncWithSignin: {
      // This dialog is triggered only if there is unsynced data.
      self.actionSheetCoordinator.alertStyle = UIAlertControllerStyleAlert;
      NSString* const signOutButtonTitle = l10n_util::GetNSString(
          IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_AND_DELETE_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:signOutButtonTitle
                    action:^{
                      base::UmaHistogramBoolean("Sync.SignoutWithUnsyncedData",
                                                true);
                      [weakSelf handleSignOutWithForceClearData:NO];
                      [weakSelf dismissActionSheetCoordinator];
                    }
                     style:UIAlertActionStyleDestructive];
      [self.actionSheetCoordinator
          addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                    action:^{
                      base::UmaHistogramBoolean("Sync.SignoutWithUnsyncedData",
                                                false);
                      [weakSelf callCompletionBlock:NO];
                      [weakSelf dismissActionSheetCoordinator];
                    }
                     style:UIAlertActionStyleCancel];
      [self.actionSheetCoordinator start];
      return;
    }
    case SignedInUserStateWithForcedSigninInfoRequired: {
      NSString* const signOutButtonTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:signOutButtonTitle
                    action:^{
                      weakSelf.confirmSignOut = YES;
                      // Stop the current action sheet coordinator and start a
                      // new one for the next step.
                      [weakSelf dismissActionSheetCoordinator];
                      [weakSelf startActionSheetCoordinatorForSignout];
                    }
                     style:UIAlertActionStyleDestructive];
      break;
    }
    case SignedInUserStateWithManagedAccountAndSyncing: {
      NSString* const clearFromDeviceTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:clearFromDeviceTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:YES];
                      [weakSelf dismissActionSheetCoordinator];
                    }
                     style:UIAlertActionStyleDestructive];
      break;
    }
    case SignedInUserStateWithManagedAccountAndNotSyncing: {
      NSString* const clearFromDeviceTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:clearFromDeviceTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:NO];
                      [weakSelf dismissActionSheetCoordinator];
                    }
                     style:UIAlertActionStyleDestructive];
      break;
    }
    case SignedInUserStateWithNonManagedAccountAndSyncing: {
      NSString* const clearFromDeviceTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON);
      NSString* const keepOnDeviceTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_KEEP_DATA_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:clearFromDeviceTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:YES];
                      [weakSelf dismissActionSheetCoordinator];
                    }
                     style:UIAlertActionStyleDestructive];
      [self.actionSheetCoordinator
          addItemWithTitle:keepOnDeviceTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:NO];
                      [weakSelf dismissActionSheetCoordinator];
                    }
                     style:UIAlertActionStyleDefault];
      break;
    }
    case SignedInUserStateWithNoneManagedAccountAndNotSyncing: {
      NSString* const signOutButtonTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:signOutButtonTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:NO];
                      [weakSelf dismissActionSheetCoordinator];
                    }
                     style:UIAlertActionStyleDestructive];
      break;
    }
  }
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^{
                  [weakSelf callCompletionBlock:NO];
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleCancel];
  [self.actionSheetCoordinator start];
}

// Signs the user out of the primary account and clears the data from their
// device if specified to do so.
- (void)handleSignOutWithForceClearData:(BOOL)forceClearData {
  if (!self.browser)
    return;

  if (!self.authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    [self callCompletionBlock:YES];
    return;
  }
  [self preventUserInteraction];
  id<SnackbarCommands> snackbarCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  // The snackbar message might be nil if the snackbar is not needed.
  MDCSnackbarMessage* snackbarMessage = [self signoutSnackbarMessage];
  __weak __typeof(self) weakSelf = self;
  self.authenticationService->SignOut(_signout_source_metric, forceClearData, ^{
    // The snackbar should be displayed even if self has been deallocated.
    [snackbarCommandsHandler showSnackbarMessage:snackbarMessage
                                    bottomOffset:0];
    [weakSelf signOutDidFinish];
  });
  // Get UMA metrics on the usage of different options for signout available
  // for users with non-managed accounts.
  if (!self.authenticationService->HasPrimaryIdentityManaged(
          signin::ConsentLevel::kSignin)) {
    UMA_HISTOGRAM_BOOLEAN("Signin.UserRequestedWipeDataOnSignout",
                          forceClearData);
  }
  signin_metrics::RecordSignoutUserAction(forceClearData);
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
  switch (self.signedInUserState) {
    case SignedInUserStateWithNotSyncingAndReplaceSyncWithSignin:
      break;
    case SignedInUserStateWithManagedAccountAndSyncing:
    case SignedInUserStateWithManagedAccountAndNotSyncing:
    case SignedInUserStateWithNonManagedAccountAndSyncing:
    case SignedInUserStateWithNoneManagedAccountAndNotSyncing:
    case SignedInUserStateWithForcedSigninInfoRequired:
      return nil;
  }
  if (self.isForceSigninEnabled) {
    // Snackbar should be skipped since force sign-in dialog will be shown right
    // after.
    return nil;
  }
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());
  int message_id =
      syncService->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
              HasManagedSyncDataType(syncService)
          ? IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_SNACKBAR_MESSAGE_ENTERPRISE
          : IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_SNACKBAR_MESSAGE;
  return
      [MDCSnackbarMessage messageWithText:l10n_util::GetNSString(message_id)];
}

// Calls `self.completion` if available, and sets it to `null` before the call.
- (void)callCompletionBlock:(BOOL)signedOut {
  if (!self.completion) {
    return;
  }
  signin_ui::CompletionCallback completion = self.completion;
  self.completion = nil;
  completion(signedOut);
}

@end
