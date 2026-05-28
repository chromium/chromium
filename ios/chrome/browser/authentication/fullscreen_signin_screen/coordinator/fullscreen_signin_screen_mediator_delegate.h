// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_FULLSCREEN_SIGNIN_SCREEN_COORDINATOR_FULLSCREEN_SIGNIN_SCREEN_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_FULLSCREEN_SIGNIN_SCREEN_COORDINATOR_FULLSCREEN_SIGNIN_SCREEN_MEDIATOR_DELEGATE_H_

@class FullscreenSigninScreenMediator;

@protocol FullscreenSigninScreenMediatorDelegate <NSObject>

// Let the coordinator know the sign-in ended successfully.
// Causes the coordinator to be stopped synchronously.
- (void)fullscreenSigninScreenMediatorDidFinishSignin:
    (FullscreenSigninScreenMediator*)mediator;

// Let the coordinator know that sign-in dialog can be dismissed.
// Causes the coordinator to be stopped synchronously.
- (void)fullscreenSigninScreenMediatorWantsToBeDismissed:
    (FullscreenSigninScreenMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_FULLSCREEN_SIGNIN_SCREEN_COORDINATOR_FULLSCREEN_SIGNIN_SCREEN_MEDIATOR_DELEGATE_H_
