// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_POLICY_SIGNOUT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_POLICY_SIGNOUT_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol PolicySignoutPromptCommands;

// Coordinates the user sign-out prompt when the user is signed out due to
// the BrowserSignin policy disabling browser sign-in.
@interface UserPolicySignoutCoordinator : ChromeCoordinator

// Handler for all actions of this coordinator.
@property(nonatomic, weak) id<PolicySignoutPromptCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_POLICY_SIGNOUT_COORDINATOR_H_
