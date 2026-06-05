// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_DEEPLINK_SIGNIN_DEEPLINK_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_DEEPLINK_SIGNIN_DEEPLINK_SIGNIN_COORDINATOR_H_

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"

@class ScreenProvider;

// Coordinator for signing users in using the `selectedAccountEmail`. If the
// account doesn't exist it will start the add account flow with the email
// prefilled.
@interface DeeplinkSigninCoordinator : SigninCoordinator

// Initiate the coordinator.
// `browser` used for authentication. It must not be off the record (incognito).
// `selectedAccountEmail` is used to select a specific account with the given
//     email. This will start the add account flow if the account is not
//     present in device. If there are multiple accounts on device the selected
//     account will be shown as chosen in account picker flow.
- (instancetype)
           initWithBaseViewController:(UIViewController*)viewController
                              browser:(Browser*)browser
                 selectedAccountEmail:(NSString*)selectedAccountEmail
    changeProfileContinuationProvider:(const ChangeProfileContinuationProvider&)
                                          changeProfileContinuationProvider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              contextStyle:(SigninContextStyle)contextStyle
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_DEEPLINK_SIGNIN_DEEPLINK_SIGNIN_COORDINATOR_H_
