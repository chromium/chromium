// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_TEST_AUTHENTICATION_FLOW_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_TEST_AUTHENTICATION_FLOW_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile_continuation_provider.h"
#import "ios/chrome/browser/signin/model/constants.h"

@interface TestAuthenticationFlowDelegate
    : NSObject <AuthenticationFlowDelegate>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSigninCompletionCallback:
                    (signin_ui::SigninCompletionCallback)signinCompletion
               changeProfileContinuationProvider:
                   (const ChangeProfileContinuationProvider&)
                       changeProfileContinuationProvider
    NS_DESIGNATED_INITIALIZER;

// Fail if this mock is used with a change of profile.
- (instancetype)initWithSigninCompletionCallback:
    (signin_ui::SigninCompletionCallback)signinCompletion;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_TEST_AUTHENTICATION_FLOW_DELEGATE_H_
