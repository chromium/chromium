// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_LOGGING_FULLSCREEN_SIGNIN_PROMO_LOGGER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_LOGGING_FULLSCREEN_SIGNIN_PROMO_LOGGER_H_

#import "ios/chrome/browser/authentication/ui_bundled/signin/logging/user_signin_logger.h"

class ChromeAccountManagerService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace signin_metrics {
enum class AccessPoint;
enum class PromoAction;
}  // namespace signin_metrics

// Logs metrics for Chrome upgrade operations.
@interface FullscreenSigninPromoLogger : NSObject <SigninLogger>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                        promoAction:(signin_metrics::PromoAction)promoAction
                    identityManager:(signin::IdentityManager*)identityManager
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService
    NS_DESIGNATED_INITIALIZER;

// Disconnect this object.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_LOGGING_FULLSCREEN_SIGNIN_PROMO_LOGGER_H_
