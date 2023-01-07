// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_LOGGING_FIRST_RUN_SIGNIN_LOGGER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_LOGGING_FIRST_RUN_SIGNIN_LOGGER_H_

#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/user_signin_logger.h"

// Logs metrics for Chrome first-run operations.
@interface FirstRunSigninLogger : UserSigninLogger

- (instancetype)initWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                        promoAction:(signin_metrics::PromoAction)promoAction
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService
    NS_UNAVAILABLE;

// The designated initializer.
- (instancetype)initWithPromoAction:(signin_metrics::PromoAction)promoAction
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_LOGGING_FIRST_RUN_SIGNIN_LOGGER_H_
