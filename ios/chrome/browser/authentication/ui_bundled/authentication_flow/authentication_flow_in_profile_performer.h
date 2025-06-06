// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_IN_PROFILE_PERFORMER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_IN_PROFILE_PERFORMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_base.h"

class ProfileIOS;
namespace signin_metrics {
enum class AccessPoint;
}

@protocol AuthenticationFlowInProfilePerformerDelegate;

// Performs the sign-in steps and user interactions as part of the sign-in in
// profile flow.
@interface AuthenticationFlowInProfilePerformer
    : AuthenticationFlowPerformerBase

// Initializes a new AuthenticationFlowInProfilePerformer. `delegate` will be
// notified when each step completes.
- (instancetype)initWithInProfileDelegate:
                    (id<AuthenticationFlowInProfilePerformerDelegate>)delegate
                     changeProfileHandler:
                         (id<ChangeProfileCommands>)changeProfileHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithDelegate:
                    (id<AuthenticationFlowPerformerBaseDelegate>)delegate
            changeProfileHandler:(id<ChangeProfileCommands>)changeProfileHandler
    NS_UNAVAILABLE;

// Signs `identity` with `currentProfile`.
- (void)signInIdentity:(id<SystemIdentity>)identity
         atAccessPoint:(signin_metrics::AccessPoint)accessPoint
        currentProfile:(ProfileIOS*)currentProfile;

// Signs out of `profile` and sends `didSignOutForAccountSwitch` to the delegate
// when complete.
- (void)signOutForAccountSwitchWithProfile:(ProfileIOS*)profile;

// Immediately signs out `profile` without waiting for dependent services.
- (void)signOutImmediatelyFromProfile:(ProfileIOS*)profile;

- (void)registerUserPolicy:(ProfileIOS*)profile
               forIdentity:(id<SystemIdentity>)identity;

// Fetches user policies asynchronously without knowing when the data will be
// available.
- (void)fetchUserPolicy:(ProfileIOS*)profile
            withDmToken:(NSString*)dmToken
               clientID:(NSString*)clientID
     userAffiliationIDs:(NSArray<NSString*>*)userAffiliationIDs
               identity:(id<SystemIdentity>)identity;

- (void)fetchAccountCapabilities:(ProfileIOS*)profile;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_IN_PROFILE_PERFORMER_H_
