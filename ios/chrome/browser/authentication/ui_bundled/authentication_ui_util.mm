// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"

#import "base/check.h"
#import "base/format_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/browser_sync/sync_to_signin_migration.h"
#import "components/signin/public/base/gaia_id_hash.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/account_pref_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Returns the title associated to the given user sign-in state.
// `account_profile_switch` is true if the flow was triggered for an account or
// profile switching.
// `signed_in_user_state` sign-in&sync state for the current primary account.
NSString* GetActionSheetCoordinatorTitle(
    signin::IdentityManager* identity_manager,
    SignedInUserState signed_in_user_state,
    bool account_profile_switch) {
  switch (signed_in_user_state) {
    case SignedInUserState::kNotSyncingAndReplaceSyncWithSignin:
      // This dialog is triggered only if there is unsync data.
      return l10n_util::GetNSString(
          IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_AND_DELETE_TITLE);
    case SignedInUserState::kManagedAccountAndMigratedFromSyncing: {
      std::u16string hostedDomain =
          HostedDomainForPrimaryAccount(identity_manager);
      return l10n_util::GetNSStringF(
          IDS_IOS_SIGNOUT_DIALOG_TITLE_WITH_SYNCING_MANAGED_ACCOUNT,
          hostedDomain);
    }
    case SignedInUserState::kManagedAccountClearsDataOnSignout: {
      return account_profile_switch
                 ? l10n_util::GetNSString(
                       IDS_IOS_SWITCH_CLEARS_DATA_DIALOG_TITLE_WITH_MANAGED_ACCOUNT)
                 : l10n_util::GetNSString(
                       IDS_IOS_SIGNOUT_CLEARS_DATA_DIALOG_TITLE_WITH_MANAGED_ACCOUNT);
    }
  }
  NOTREACHED();
}

// Returns the message associated to the given user sign-in state. Can return
// nil.
// `account_profile_switch` is true if the flow was triggered for an account or
// profile switching.
// `signed_in_user_state` sign-in&sync state for the current primary account.
NSString* GetActionSheetCoordinatorMessage(
    AuthenticationService* authentication_service,
    SignedInUserState signed_in_user_state,
    bool account_profile_switch) {
  switch (signed_in_user_state) {
    case SignedInUserState::kNotSyncingAndReplaceSyncWithSignin: {
      // This dialog is triggered only if there is unsync data.
      NSString* userEmail =
          authentication_service
              ->GetPrimaryIdentity(signin::ConsentLevel::kSignin)
              .userEmail;
      return account_profile_switch
                 ? l10n_util::GetNSStringF(
                       IDS_IOS_DATA_NOT_UPLOADED_SWITCH_DIALOG_BODY,
                       base::SysNSStringToUTF16(userEmail))
                 : l10n_util::GetNSString(
                       IDS_IOS_SIGNOUT_DIALOG_MESSAGE_WITH_NOT_SAVED_DATA);
    }
    case SignedInUserState::kManagedAccountClearsDataOnSignout:
      // If `kIdentityDiscAccountMenu` is enabled, signing out may also cause
      // tabs to be closed, see `MainControllerAuthenticationServiceDelegate::
      //    ClearBrowsingDataForSignedinPeriod`.
      return IsIdentityDiscAccountMenuEnabled()
                 ? l10n_util::GetNSString(
                       IDS_IOS_SIGNOUT_CLOSES_TABS_AND_CLEARS_DATA_DIALOG_MESSAGE_WITH_MANAGED_ACCOUNT)
                 : l10n_util::GetNSString(
                       IDS_IOS_SIGNOUT_CLEARS_DATA_DIALOG_MESSAGE_WITH_MANAGED_ACCOUNT);
    case SignedInUserState::kManagedAccountAndMigratedFromSyncing: {
      return nil;
    }
  }
  NOTREACHED();
}

}  // namespace

std::u16string HostedDomainForPrimaryAccount(
    signin::IdentityManager* identity_manager) {
  return base::UTF8ToUTF16(
      identity_manager
          ->FindExtendedAccountInfo(identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin))
          .hosted_domain);
}

AlertCoordinator* ErrorCoordinator(NSError* error,
                                   ProceduralBlock dismissAction,
                                   UIViewController* viewController,
                                   Browser* browser) {
  DCHECK(error);

  AlertCoordinator* alertCoordinator =
      ErrorCoordinatorNoItem(error, viewController, browser);

  NSString* okButtonLabel = l10n_util::GetNSString(IDS_OK);
  [alertCoordinator addItemWithTitle:okButtonLabel
                              action:dismissAction
                               style:UIAlertActionStyleDefault];

  alertCoordinator.noInteractionAction = dismissAction;

  return alertCoordinator;
}

NSString* DialogMessageFromError(NSError* error) {
  NSMutableString* errorMessage = [[NSMutableString alloc] init];
  if (error.userInfo[NSLocalizedDescriptionKey]) {
    [errorMessage appendString:error.localizedDescription];
  } else {
    [errorMessage appendString:@"--"];
  }
  [errorMessage appendString:@" ("];
  NSError* errorCursor = error;
  for (int errorDepth = 0; errorDepth < 3 && errorCursor; ++errorDepth) {
    if (errorDepth > 0) {
      [errorMessage appendString:@", "];
    }
    [errorMessage
        appendFormat:@"%@: %" PRIdNS, errorCursor.domain, errorCursor.code];
    errorCursor = errorCursor.userInfo[NSUnderlyingErrorKey];
  }
  [errorMessage appendString:@")"];
  return [errorMessage copy];
}

AlertCoordinator* ErrorCoordinatorNoItem(NSError* error,
                                         UIViewController* viewController,
                                         Browser* browser) {
  DCHECK(error);

  NSString* title = l10n_util::GetNSString(
      IDS_IOS_SYNC_AUTHENTICATION_ERROR_ALERT_VIEW_TITLE);
  NSString* errorMessage;
  if ([NSURLErrorDomain isEqualToString:error.domain] &&
      error.code == kCFURLErrorCannotConnectToHost) {
    errorMessage =
        l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_INTERNET_DISCONNECTED);
  } else {
    errorMessage = DialogMessageFromError(error);
  }
  AlertCoordinator* alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:viewController
                                                   browser:browser
                                                     title:title
                                                   message:errorMessage];
  return alertCoordinator;
}

NSString* ViewControllerPresentationStatusDescription(
    UIViewController* view_controller) {
  if (!view_controller) {
    return @"No view controller";
  } else if (view_controller.isBeingPresented) {
    return @"Being presented";
  } else if (view_controller.isBeingDismissed) {
    return @"Being dismissed";
  } else if (view_controller.presentingViewController) {
    return [NSString stringWithFormat:@"Presented by: %@",
                                      view_controller.presentingViewController];
  }
  return @"Not presented";
}

AlertCoordinator* ManagedConfirmationDialogContentForHostedDomain(
    NSString* hosted_domain,
    Browser* browser,
    UIViewController* view_controller,
    ProceduralBlock accept_block,
    ProceduralBlock cancel_block) {
  NSString* title = l10n_util::GetNSString(IDS_IOS_MANAGED_SIGNIN_TITLE);
  NSString* subtitle =
      l10n_util::GetNSStringF(IDS_IOS_MANAGED_SIGNIN_WITH_USER_POLICY_SUBTITLE,
                              base::SysNSStringToUTF16(hosted_domain));
  NSString* accept_label = l10n_util::GetNSString(
      IDS_IOS_MANAGED_SIGNIN_WITH_USER_POLICY_CONTINUE_BUTTON_LABEL);
  NSString* cancel_label = l10n_util::GetNSString(IDS_CANCEL);

  AlertCoordinator* managed_confirmation_alert_coordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:view_controller
                                                   browser:browser
                                                     title:title
                                                   message:subtitle];

  [managed_confirmation_alert_coordinator
      addItemWithTitle:cancel_label
                action:cancel_block
                 style:UIAlertActionStyleCancel];
  [managed_confirmation_alert_coordinator
      addItemWithTitle:accept_label
                action:accept_block
                 style:UIAlertActionStyleDefault];
  managed_confirmation_alert_coordinator.noInteractionAction = cancel_block;
  [managed_confirmation_alert_coordinator start];
  return managed_confirmation_alert_coordinator;
}

namespace {

// Returns yes if the browser has machine level policies.
bool HasMachineLevelPolicies() {
  BrowserPolicyConnectorIOS* policy_connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  return policy_connector && policy_connector->HasMachineLevelPolicies();
}

}  // namespace

BOOL ShouldShowManagedConfirmationForHostedDomain(
    NSString* hosted_domain,
    signin_metrics::AccessPoint access_point,
    NSString* gaia_id,
    PrefService* prefs) {
  if ([hosted_domain length] == 0) {
    // No hosted domain, don't show the dialog as there is no host.
    return NO;
  }

  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    if (HasMachineLevelPolicies()) {
      // Don't show the dialog if the browser has already machine level policies
      // as the user already knows that their browser is managed.
      return NO;
    }

    signin::GaiaIdHash gaia_id_hash =
        signin::GaiaIdHash::FromGaiaId(GaiaId(gaia_id));
    const base::Value* already_seen = syncer::GetAccountKeyedPrefValue(
        prefs, prefs::kSigninHasAcceptedManagementDialog, gaia_id_hash);

    if (already_seen && already_seen->GetIfBool().value_or(false)) {
      return NO;
    }
  } else if (GetApplicationContext()
                 ->GetAccountProfileMapper()
                 ->IsProfileForGaiaIDFullyInitialized(GaiaId(gaia_id))) {
    // If the corresponding profile is fully initialized, the user has
    // already seen the confirmation screen.
    return NO;
  }

  return YES;
}

SignedInUserState GetSignedInUserState(
    AuthenticationService* authentication_service,
    signin::IdentityManager* identity_manager,
    PrefService* profile_pref_service) {
  const bool is_managed_account_migrated_from_syncing =
      browser_sync::WasPrimaryAccountMigratedFromSyncingToSignedIn(
          identity_manager, profile_pref_service) &&
      authentication_service->HasPrimaryIdentityManaged(
          signin::ConsentLevel::kSignin);

  if (is_managed_account_migrated_from_syncing) {
    return SignedInUserState::kManagedAccountAndMigratedFromSyncing;
  }
  if (authentication_service->ShouldClearDataForSignedInPeriodOnSignOut()) {
    return SignedInUserState::kManagedAccountClearsDataOnSignout;
  }
  return SignedInUserState::kNotSyncingAndReplaceSyncWithSignin;
}

bool ForceLeavingPrimaryAccountConfirmationDialog(
    SignedInUserState signed_in_user_state,
    std::string_view profile_name) {
  switch (signed_in_user_state) {
    case SignedInUserState::kNotSyncingAndReplaceSyncWithSignin:
      return false;
    case SignedInUserState::kManagedAccountClearsDataOnSignout:
    case SignedInUserState::kManagedAccountAndMigratedFromSyncing:
      if (!AreSeparateProfilesForManagedAccountsEnabled()) {
        return true;
      }

      // Show the dialog only if a managed account is signing out from the
      // personal profile. (This can only happen for managed accounts that were
      // already signed in before there was multi-profile support.)
      return GetApplicationContext()
                 ->GetProfileManager()
                 ->GetProfileAttributesStorage()
                 ->GetPersonalProfileName() == profile_name;
  }
  NOTREACHED();
}

ActionSheetCoordinator* GetLeavingPrimaryAccountConfirmationDialog(
    UIViewController* base_view_controller,
    Browser* browser,
    UIView* anchor_view,
    CGRect anchor_rect,
    SignedInUserState signed_in_user_state,
    bool account_profile_switch,
    LeavingPrimaryAccountConfirmationDialogCompletion completion) {
  ProfileIOS* profile = browser->GetProfile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  NSString* title = GetActionSheetCoordinatorTitle(
      identity_manager, signed_in_user_state, account_profile_switch);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  NSString* message = GetActionSheetCoordinatorMessage(
      authentication_service, signed_in_user_state, account_profile_switch);
  ActionSheetCoordinator* actionSheetCoordinator =
      [[ActionSheetCoordinator alloc]
          initWithBaseViewController:base_view_controller
                             browser:browser
                               title:title
                             message:message
                                rect:anchor_rect
                                view:anchor_view];
  switch (signed_in_user_state) {
    case SignedInUserState::kNotSyncingAndReplaceSyncWithSignin: {
      // This dialog is triggered only if there is unsynced data.
      actionSheetCoordinator.alertStyle = UIAlertControllerStyleAlert;
      NSString* const signOutButtonTitle =
          account_profile_switch
              ? l10n_util::GetNSString(
                    IDS_IOS_DATA_NOT_UPLOADED_SWITCH_DIALOG_BUTTON)
              : l10n_util::GetNSString(
                    IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_AND_DELETE_BUTTON);
      [actionSheetCoordinator
          addItemWithTitle:signOutButtonTitle
                    action:^{
                      base::RecordAction(base::UserMetricsAction(
                          "Signin_Signout_Confirm_Regular_UNO"));
                      signin_metrics::
                          RecordSignoutConfirmationFromDataLossAlert(
                              signin_metrics::SignoutDataLossAlertReason::
                                  kSignoutWithUnsyncedData,
                              true);
                      completion(YES);
                    }
                     style:UIAlertActionStyleDestructive];
      [actionSheetCoordinator
          addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                    action:^{
                      base::RecordAction(base::UserMetricsAction(
                          "Signin_Signout_Cancel_Regular_UNO"));
                      signin_metrics::
                          RecordSignoutConfirmationFromDataLossAlert(
                              signin_metrics::SignoutDataLossAlertReason::
                                  kSignoutWithUnsyncedData,
                              false);
                      completion(NO);
                    }
                     style:UIAlertActionStyleCancel];
      break;
    }
    case SignedInUserState::kManagedAccountClearsDataOnSignout: {
      actionSheetCoordinator.alertStyle = UIAlertControllerStyleAlert;
      NSString* const signOutButtonTitle =
          account_profile_switch
              ? l10n_util::GetNSString(
                    IDS_IOS_DATA_NOT_UPLOADED_SWITCH_DIALOG_BUTTON)
              : l10n_util::GetNSString(
                    IDS_IOS_SIGNOUT_AND_DELETE_DIALOG_SIGN_OUT_BUTTON);
      [actionSheetCoordinator
          addItemWithTitle:signOutButtonTitle
                    action:^{
                      base::RecordAction(base::UserMetricsAction(
                          "Signin_Signout_Confirm_Managed_ClearDataOnSignout"));
                      signin_metrics::
                          RecordSignoutConfirmationFromDataLossAlert(
                              signin_metrics::SignoutDataLossAlertReason::
                                  kSignoutWithClearDataForManagedUser,
                              true);
                      completion(YES);
                    }
                     style:UIAlertActionStyleDestructive];
      [actionSheetCoordinator
          addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                    action:^{
                      base::RecordAction(base::UserMetricsAction(
                          "Signin_Signout_Cancel_Managed_ClearDataOnSignout"));
                      signin_metrics::
                          RecordSignoutConfirmationFromDataLossAlert(
                              signin_metrics::SignoutDataLossAlertReason::
                                  kSignoutWithClearDataForManagedUser,
                              false);
                      completion(NO);
                    }
                     style:UIAlertActionStyleCancel];
      break;
    }
    case SignedInUserState::kManagedAccountAndMigratedFromSyncing: {
      if (IsIdentityDiscAccountMenuEnabled()) {
        actionSheetCoordinator.alertStyle = UIAlertControllerStyleAlert;
      }
      NSString* const clearFromDeviceTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON);
      [actionSheetCoordinator
          addItemWithTitle:clearFromDeviceTitle
                    action:^{
                      base::RecordAction(base::UserMetricsAction(
                          "Signin_Signout_Confirm_Managed_Syncing"));
                      completion(YES);
                    }
                     style:UIAlertActionStyleDestructive];
      [actionSheetCoordinator
          addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                    action:^{
                      base::RecordAction(
                          base::UserMetricsAction("Signin_Signout_Cancel"));
                      completion(NO);
                    }
                     style:UIAlertActionStyleCancel];
      break;
    }
  }
  return actionSheetCoordinator;
}
