// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_IN_PROGRESS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_IN_PROGRESS_H_

#import <Foundation/Foundation.h>

@protocol ApplicationCommands;
class SigninInProgress;

@protocol SignInInProgressAudience <NSObject>

// Records that an extra sign-in process started.
- (void)signInStarted;

// Records that a sign-in is done.
- (void)signinFinished;

@end

// While an object of this class is alive, the SceneController knows
// a sign-in operation is in progress and blocks the UI of the other scenes.
class SigninInProgress {
 public:
  explicit SigninInProgress(id<SignInInProgressAudience> audience);
  ~SigninInProgress();

 private:
  // The audience.
  __weak id<SignInInProgressAudience> audience_;
};

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_IN_PROGRESS_H_
