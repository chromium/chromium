// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"

#import "base/check.h"
#import "base/format_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/signin/public/base/gaia_id_hash.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/account_pref_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_switch.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

std::u16string HostedDomainForPrimaryAccount(Browser* browser) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser->GetProfile());
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
  // Show the legacy managed confirmation dialog if User Policy is disabled.
  // Otherwise, show the release version of the managed confirmation dialog for
  // User Policy if User Policy is enabled and there is no Sync consent.
  bool user_policy_enabled = policy::IsAnyUserPolicyFeatureEnabled();
  NSString* title = l10n_util::GetNSString(IDS_IOS_MANAGED_SIGNIN_TITLE);
  NSString* subtitle = l10n_util::GetNSStringF(
      user_policy_enabled ? IDS_IOS_MANAGED_SIGNIN_WITH_USER_POLICY_SUBTITLE
                          : IDS_IOS_MANAGED_SIGNIN_SUBTITLE,
      base::SysNSStringToUTF16(hosted_domain));
  NSString* accept_label = l10n_util::GetNSString(
      user_policy_enabled
          ? IDS_IOS_MANAGED_SIGNIN_WITH_USER_POLICY_CONTINUE_BUTTON_LABEL
          : IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON);
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

  if (HasMachineLevelPolicies()) {
    // Don't show the dialog if the browser has already machine level policies
    // as the user already knows that their browser is managed.
    return NO;
  }

  if (access_point == signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_MENU &&
      base::FeatureList::IsEnabled(kIdentityDiscAccountMenu)) {
    // Only show the dialog once per account, when switching from the Account
    // Menu.
    signin::GaiaIdHash gaia_id_hash =
        signin::GaiaIdHash::FromGaiaId(base::SysNSStringToUTF8(gaia_id));
    const base::Value* already_seen = syncer::GetAccountKeyedPrefValue(
        prefs, prefs::kSigninHasAcceptedManagementDialog, gaia_id_hash);
    if (already_seen && already_seen->GetIfBool().value_or(false)) {
      return NO;
    }
  }

  // Show the dialog if User Policy is enabled.
  return policy::IsAnyUserPolicyFeatureEnabled();
}
