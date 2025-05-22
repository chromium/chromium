// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_BASE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_BASE_H_

#import <UIKit/UIKit.h>

#import "base/functional/callback_forward.h"
#import "base/ios/block_types.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/signin/model/constants.h"

@protocol AuthenticationFlowPerformerBaseDelegate;
class Browser;
enum class ChangeProfileReason;
@protocol ChangeProfileCommands;
@class SceneState;
@protocol SystemIdentity;

namespace signin_metrics {
enum class AccessPoint;
}

// Completes the post-signin actions. In most cases the action is showing a
// snackbar confirming sign-in with `identity` and an undo button to sign out
// the user.
void CompletePostSignInActions(PostSignInActionSet post_signin_actions,
                               id<SystemIdentity> identity,
                               Browser* browser,
                               signin_metrics::AccessPoint access_point);

// Callback called the profile switching succeeded (`success` is true) or failed
// (`success` is false).
// If `success is true:
// `browser` is the browser of the new profile.
using OnProfileSwitchCompletion =
    base::OnceCallback<void(bool success, Browser* new_profile_browser)>;

// Performs the sign-in steps and user interactions as part of the sign-in flow.
@interface AuthenticationFlowPerformerBase : NSObject

// Initializes a new AuthenticationFlowPerformerBase. `delegate` will be
// notified when each step completes.
- (instancetype)initWithDelegate:
                    (id<AuthenticationFlowPerformerBaseDelegate>)delegate
            changeProfileHandler:(id<ChangeProfileCommands>)changeProfileHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Switches to the profile with `profileName`, for `sceneIdentifier`.
- (void)switchToProfileWithName:(const std::string&)profileName
                     sceneState:(SceneState*)sceneState
                         reason:(ChangeProfileReason)reason
      changeProfileContinuation:(ChangeProfileContinuation)continuation
              postSignInActions:(PostSignInActionSet)postSignInActions
                   withIdentity:(id<SystemIdentity>)identity
                    accessPoint:(signin_metrics::AccessPoint)accessPoint;

// Shows `error` to the user and calls `callback` on dismiss.
- (void)showAuthenticationError:(NSError*)error
                 withCompletion:(ProceduralBlock)callback
                 viewController:(UIViewController*)viewController
                        browser:(Browser*)browser;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_BASE_H_
