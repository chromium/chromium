// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_SIGNIN_SIGNIN_SCREEN_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_SIGNIN_SIGNIN_SCREEN_MEDIATOR_DELEGATE_H_

@class SigninScreenMediator;

@protocol SigninScreenMediatorDelegate <NSObject>

// Let the coordinator know the sing-in ended successfully.
- (void)mediatorFinishedSignin:(SigninScreenMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_SIGNIN_SIGNIN_SCREEN_MEDIATOR_DELEGATE_H_
