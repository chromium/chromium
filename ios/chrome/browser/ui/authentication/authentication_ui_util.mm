// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"

#import "base/check.h"
#import "base/format_macros.h"
#import "base/strings/utf_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Enum to described all 4 cases for a user being signed-in. This enum is used
// internaly by SignoutActionSheetCoordinator().
typedef NS_ENUM(NSUInteger, SignedInUserState) {
  // Sign-in with a managed account and sync is turned on.
  SignedInUserStateWithManagedAccountAndSyncing,
  // Sign-in with a managed account and sync is turned off.
  SignedInUserStateWithManagedAccountAndNotSyncing,
  // Sign-in with a regular account and sync is turned on.
  SignedInUserStateWithNonManagedAccountAndSyncing,
  // Sign-in with a regular account and sync is turned off.
  SignedInUserStateWithNoneManagedAccountAndNotSyncing
};

base::string16 HostedDomainForPrimaryAccount(Browser* browser) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser->GetBrowserState());
  base::Optional<AccountInfo> account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager->GetPrimaryAccountInfo());
  std::string hosted_domain = account_info.has_value()
                                  ? account_info.value().hosted_domain
                                  : std::string();
  return base::UTF8ToUTF16(hosted_domain);
}

ActionSheetCoordinator* SignoutActionSheetCoordinator(
    UIViewController* view_controller,
    Browser* browser,
    UIView* view,
    SignoutActionSheetCoordinatorCompletion signout_completion) {
  DCHECK(signout_completion);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          browser->GetBrowserState());
  NSString* title = nil;
  NSString* message = nil;
  SyncSetupService* sync_setup_service =
      SyncSetupServiceFactory::GetForBrowserState(browser->GetBrowserState());
  BOOL sync_enabled = sync_setup_service->IsFirstSetupComplete();
  SignedInUserState signed_in_user_state;
  if (authentication_service->IsAuthenticatedIdentityManaged()) {
    signed_in_user_state =
        sync_enabled ? SignedInUserStateWithManagedAccountAndSyncing
                     : SignedInUserStateWithManagedAccountAndNotSyncing;
  } else {
    signed_in_user_state =
        sync_enabled ? SignedInUserStateWithNonManagedAccountAndSyncing
                     : SignedInUserStateWithNoneManagedAccountAndNotSyncing;
  }
  switch (signed_in_user_state) {
    case SignedInUserStateWithManagedAccountAndSyncing:
    case SignedInUserStateWithManagedAccountAndNotSyncing: {
      title = l10n_util::GetNSString(
          IDS_IOS_SIGNOUT_DIALOG_TITLE_WITH_MANAGED_ACCOUNT);
      base::string16 hosted_domain = HostedDomainForPrimaryAccount(browser);
      message = l10n_util::GetNSStringF(
          IDS_IOS_SIGNOUT_DIALOG_MESSAGE_WITH_MANAGED_ACCOUNT, hosted_domain);
      break;
    }
    case SignedInUserStateWithNonManagedAccountAndSyncing: {
      title = l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_TITLE_WITH_SYNC);
      message =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_MESSAGE_WITH_SYNC);
      break;
    }
    case SignedInUserStateWithNoneManagedAccountAndNotSyncing: {
      title = l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_TITLE_WITHOUT_SYNC);
      break;
    }
  }
  ActionSheetCoordinator* alertCoordinator =
      [[ActionSheetCoordinator alloc] initWithBaseViewController:view_controller
                                                         browser:browser
                                                           title:title
                                                         message:message
                                                            rect:view.frame
                                                            view:view];
  switch (signed_in_user_state) {
    case SignedInUserStateWithManagedAccountAndSyncing:
    case SignedInUserStateWithManagedAccountAndNotSyncing: {
      NSString* const clear_from_this_device =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON);
      [alertCoordinator
          addItemWithTitle:clear_from_this_device
                    action:^{
                      signout_completion(
                          SignoutActionSheetCoordinatorResultClearFromDevice);
                    }
                     style:UIAlertActionStyleDestructive];
      break;
    }
    case SignedInUserStateWithNonManagedAccountAndSyncing: {
      NSString* const clear_from_this_device =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON);
      NSString* const keep_on_this_device =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_KEEP_DATA_BUTTON);
      [alertCoordinator
          addItemWithTitle:clear_from_this_device
                    action:^{
                      signout_completion(
                          SignoutActionSheetCoordinatorResultClearFromDevice);
                    }
                     style:UIAlertActionStyleDestructive];
      [alertCoordinator
          addItemWithTitle:keep_on_this_device
                    action:^{
                      signout_completion(
                          SignoutActionSheetCoordinatorResultKeepOnDevice);
                    }
                     style:UIAlertActionStyleDefault];
      break;
    }
    case SignedInUserStateWithNoneManagedAccountAndNotSyncing: {
      NSString* const sign_out =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON);
      [alertCoordinator
          addItemWithTitle:sign_out
                    action:^{
                      signout_completion(
                          SignoutActionSheetCoordinatorResultKeepOnDevice);
                    }
                     style:UIAlertActionStyleDestructive];
      break;
    }
  }
  [alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^{
                  signout_completion(
                      SignoutActionSheetCoordinatorResultCanceled);
                }
                 style:UIAlertActionStyleCancel];
  return alertCoordinator;
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

  [alertCoordinator setCancelAction:dismissAction];

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
