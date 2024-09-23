// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_LOGGING_USER_SIGNIN_LOGGER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_LOGGING_USER_SIGNIN_LOGGER_H_

#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

// Logs metrics for user sign-in operations.
@interface UserSigninLogger : NSObject

- (instancetype)init NS_UNAVAILABLE;
// The designated initializer.
- (instancetype)initWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                        promoAction:(signin_metrics::PromoAction)promoAction
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService
    NS_DESIGNATED_INITIALIZER;

// View where the sign-in button was displayed.
@property(nonatomic, assign, readonly) signin_metrics::AccessPoint accessPoint;

// Promo button used to trigger the sign-in.
@property(nonatomic, assign, readonly) signin_metrics::PromoAction promoAction;

// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

// Disconnect this object.
- (void)disconnect;

// Logs sign-in started when the user consent screen is first displayed.
- (void)logSigninStarted;

// Logs sign-in completed when the user has attempted sign-in and obtained a
// result.
- (void)logSigninCompletedWithResult:(SigninCoordinatorResult)signinResult
                        addedAccount:(BOOL)addedAccount;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_LOGGING_USER_SIGNIN_LOGGER_H_
