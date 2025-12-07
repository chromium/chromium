// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_LOGGING_SIGNIN_LOGGER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_LOGGING_SIGNIN_LOGGER_H_

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSUInteger, SigninCoordinatorResult);

// Logs metrics for sign-in operations.
@protocol SigninLogger <NSObject>

// Logs sign-in started metrics, should be called when the sign-in UI is first
// displayed.
- (void)logSigninStarted;

// Logs sign-in completed. Should be called when the user has attempted sign-in
// and obtained a result.
- (void)logSigninCompletedWithResult:(SigninCoordinatorResult)signinResult
                        addedAccount:(BOOL)addedAccount;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_LOGGING_SIGNIN_LOGGER_H_
