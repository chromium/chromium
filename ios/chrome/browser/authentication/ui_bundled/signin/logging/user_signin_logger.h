// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_LOGGING_USER_SIGNIN_LOGGER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_LOGGING_USER_SIGNIN_LOGGER_H_

#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/logging/signin_logger.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

// SigninLogger for user-initiated sign-in flows.
@interface UserSigninLogger : NSObject <SigninLogger>

- (instancetype)init NS_UNAVAILABLE;
// The designated initializer.
- (instancetype)initWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                        promoAction:(signin_metrics::PromoAction)promoAction
    NS_DESIGNATED_INITIALIZER;

// View where the sign-in button was displayed.
@property(nonatomic, assign, readonly) signin_metrics::AccessPoint accessPoint;

// Promo button used to trigger the sign-in.
@property(nonatomic, assign, readonly) signin_metrics::PromoAction promoAction;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_LOGGING_USER_SIGNIN_LOGGER_H_
