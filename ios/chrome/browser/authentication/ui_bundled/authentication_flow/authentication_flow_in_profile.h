// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_IN_PROFILE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_IN_PROFILE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/signin/model/constants.h"

class Browser;
@protocol SystemIdentity;

namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics

// `AuthenticationFlowInProfile` manages the authentication flow for a given
// identity. This identity must be assigned with the current profile.
// This class is meant to only be used by `AuthenticationFlow` class.
// If the identity is assigned to another profile, the profile switching must be
// done before creating this instance.
// This class auto-retains itself until the sign-in is done.
@interface AuthenticationFlowInProfile : NSObject

// Designated initializer.
// `browser` is the browser of the current profile.
// `identity` to sign in. This identity must be assigned the current profile.
// `precedingHistorySync` specifies whether the History Sync Opt-In screen
//   follows after authentication flow completes with success.
- (instancetype)initWithBrowser:(Browser*)browser
                       identity:(id<SystemIdentity>)identity
              isManagedIdentity:(BOOL)isManagedIdentity
                    accessPoint:(signin_metrics::AccessPoint)accessPoint
           precedingHistorySync:(BOOL)precedingHistorySync
              postSignInActions:(PostSignInActionSet)postSignInActions
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Starts the sign-in flow. `AuthenticationFlowInProfile` retains itself until
// the sign-in is done.
- (void)startSignInWithCompletion:
    (signin_ui::SigninCompletionCallback)completion;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_IN_PROFILE_H_
