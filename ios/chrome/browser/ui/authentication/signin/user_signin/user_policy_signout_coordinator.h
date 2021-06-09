// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_POLICY_SIGNOUT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_POLICY_SIGNOUT_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol PolicySignoutPromptCommands;

// Delegate for the coordinator
@protocol UserPolicySignoutCoordinatorDelegate

// Command to clean up the sign-out prompt. Stops the coordinator and sets it to
// nil. Should only be invoked by the prompt's action handlers. |learnMore| is
// YES if the user tapped the "learn more" button. The action is already
// handled.
- (void)hidePolicySignoutPromptForLearnMore:(BOOL)learnMore;

// Notifies the delegate that the policy Sign out screen has been dismissed.
- (void)userPolicySignoutDidDismiss;

@end

// Coordinates the user sign-out prompt when the user is signed out due to
// the BrowserSignin policy disabling browser sign-in.
@interface UserPolicySignoutCoordinator : ChromeCoordinator

// Delegate for dismissing the coordinator.
@property(nonatomic, weak) id<UserPolicySignoutCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_POLICY_SIGNOUT_COORDINATOR_H_
