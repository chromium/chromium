// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_LOGGING_UPGRADE_SIGNIN_LOGGER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_LOGGING_UPGRADE_SIGNIN_LOGGER_H_

#import "ios/chrome/browser/ui/authentication/signin/logging/user_signin_logger.h"

// Logs metrics for Chrome upgrade operations.
@interface UpgradeSigninLogger : UserSigninLogger

// TODO(crbug.com/40074532): Those 2 methods should be removed. Their
// implementation should be inside UpgradeSigninLogger. Those methods were
// exposed to fix: crbug.com/1491096. Called when the upgrade promo is opened.
// This method records metrics and preferences related to the upgrade promo.
// If `accountManagerService` is `nullptr`, this method is a nop.
+ (void)logSigninStartedWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                  accountManagerService:
                      (ChromeAccountManagerService*)accountManagerService;
// Called when the upgrade promo is done. This method records metrics and
// preferences related to the upgrade promo.
+ (void)logSigninCompletedWithResult:(SigninCoordinatorResult)signinResult
                        addedAccount:(BOOL)addedAccount;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_LOGGING_UPGRADE_SIGNIN_LOGGER_H_
