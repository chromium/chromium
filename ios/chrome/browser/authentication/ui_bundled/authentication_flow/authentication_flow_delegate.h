// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/signin/model/constants.h"

// Call back used by `-[<AuthenticationFlowDelegate>
// authenticationFlowWillSwitchProfileWithReadyCompletion:]`.
using ReadyForProfileSwitchingCompletion =
    base::OnceCallback<void(ChangeProfileContinuation)>;

@class SceneState;

// Handles callbacks for the end of the sign-in flow.
@protocol AuthenticationFlowDelegate <NSObject>

// Called at the end of the sign-in if the profile has not changed.
- (void)authenticationFlowDidSignInInSameProfileWithResult:
    (SigninCoordinatorResult)result;

// Called when the profile switching is going to happen. The delegate can
// update the UI if needed before the profile switching.
// Once the delegate is ready, `readyCompletion` needs to be called with a
// `ChangeProfileContinuation`.
// The `ChangeProfileContinuation` will be called in the new profile when it
// will be fully loaded.
- (void)authenticationFlowWillSwitchProfileWithReadyCompletion:
    (ReadyForProfileSwitchingCompletion)readyCompletion;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_DELEGATE_H_
