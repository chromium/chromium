// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_EXPECTED_SIGNIN_HISTOGRAMS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_EXPECTED_SIGNIN_HISTOGRAMS_H_

#import <Foundation/Foundation.h>

namespace signin_metrics {
enum class AccessPoint : int;
}  // namespace signin_metrics

// A data container for the expected signin histograms related to a specific
// access point.
@interface ExpectedSigninHistograms : NSObject

@property(nonatomic, readonly) signin_metrics::AccessPoint accessPoint;

// For each property p below, its value is the number of time we expect the
// histogram p to be emitted. The string p is obtained from the histogram name
// by lowering the first case and removing dots.

@property(nonatomic, assign) int signinSignInOffered;
@property(nonatomic, assign) int signinSignInOfferedWithdefault;
@property(nonatomic, assign) int signinSignInOfferedNewAccountNoExistingAccount;

@property(nonatomic, assign) int signinSigninStartedAccessPoint;
@property(nonatomic, assign) int signinSigninStartedAccessPointWithDefault;
@property(nonatomic, assign) int signinSigninStartedAccessPointNotDefault;
@property(nonatomic, assign)
    int signinSignStartedAccessPointNewAccountNoExistingAccount;
@property(nonatomic, assign)
    int signinSignStartedAccessPointNewAccountExistingAccount;

@property(nonatomic, assign) int signinSignInCompleted;
@property(nonatomic, assign) int signinSigninCompletedAccessPoint;
@property(nonatomic, assign) int signinSigninCompletedAccessPointWithDefault;
@property(nonatomic, assign) int signinSigninCompletedAccessPointNotDefault;
@property(nonatomic, assign)
    int signinSigninCompletedAccessPointNewAccountNoExistingAccount;
@property(nonatomic, assign)
    int signinSigninCompletedAccessPointNewAccountExistingAccount;

@property(nonatomic, assign) int signinSignInStarted;

- (instancetype)init NS_UNAVAILABLE;

// This object will consider histogram emitted for `accessPoint`.
- (instancetype)initWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_EXPECTED_SIGNIN_HISTOGRAMS_H_
