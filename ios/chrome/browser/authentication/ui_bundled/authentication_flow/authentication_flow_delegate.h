// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/signin/model/constants.h"

@class SceneState;

// Handles callbacks for the end of the sign-in flow.
@protocol AuthenticationFlowDelegate <NSObject>

// Called at the end of the sign-in if the profile has not changed.
- (void)authenticationFlowDidSignInInSameProfileWithResult:
    (SigninCoordinatorResult)result;

// Returns a callback to be executed once the profile is changed.
// Calling this method informs the delegate that the Authentication Flow must
// not be interrupted while the delegate is stopped.
// It must always be called before the profile switch occurred, as otherwise the
// delegate will probably be nil.
- (ChangeProfileContinuation)authenticationFlowWillChangeProfile;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_DELEGATE_H_
