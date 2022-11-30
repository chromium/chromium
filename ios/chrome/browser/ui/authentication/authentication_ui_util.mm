// Copyright 2014 The Chromium Authors
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

std::u16string HostedDomainForPrimaryAccount(Browser* browser) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser->GetBrowserState());
  return base::UTF8ToUTF16(
      identity_manager
          ->FindExtendedAccountInfo(identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSync))
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
