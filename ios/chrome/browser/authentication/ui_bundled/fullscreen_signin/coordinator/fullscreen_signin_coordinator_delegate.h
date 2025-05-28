// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_FULLSCREEN_SIGNIN_COORDINATOR_FULLSCREEN_SIGNIN_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_FULLSCREEN_SIGNIN_COORDINATOR_FULLSCREEN_SIGNIN_COORDINATOR_DELEGATE_H_

@class FullscreenSigninCoordinator;

// Delegate for the full screen signin coordinator
@protocol FullscreenSigninCoordinatorDelegate <NSObject>

// Let the delegate knows the result and request to be stopped.
- (void)fullscreenSigninCoordinatorWantsToBeStopped:
            (FullscreenSigninCoordinator*)coordinator
                                             result:(SigninCoordinatorResult)
                                                        result;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_FULLSCREEN_SIGNIN_COORDINATOR_FULLSCREEN_SIGNIN_COORDINATOR_DELEGATE_H_
