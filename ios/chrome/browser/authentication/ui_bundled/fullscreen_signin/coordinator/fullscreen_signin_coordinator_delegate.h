// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_FULLSCREEN_SIGNIN_COORDINATOR_FULLSCREEN_SIGNIN_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_FULLSCREEN_SIGNIN_COORDINATOR_FULLSCREEN_SIGNIN_COORDINATOR_DELEGATE_H_

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"

@class FullscreenSigninCoordinator;

// Delegate for the full screen signin coordinator.
@protocol FullscreenSigninCoordinatorDelegate <NSObject>

// Lets the delegate know the result and requests to be stopped.
- (void)
    fullscreenSigninCoordinatorDone:(FullscreenSigninCoordinator*)coordinator
                             result:(SigninCoordinatorResultOrIdentity)result;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_FULLSCREEN_SIGNIN_COORDINATOR_FULLSCREEN_SIGNIN_COORDINATOR_DELEGATE_H_
