// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_MEDIATOR_DELEGATE_H_

@class SigninSyncMediator;

// Delegate for SigninSyncMediator.
@protocol SigninSyncMediatorDelegate <NSObject>

// Called when sign-in did successfully finish.
- (void)signinSyncMediatorDidSuccessfulyFinishSignin:
    (SigninSyncMediator*)signinSyncMediator;

// Called when sign-in for advanced settings did successfully finish.
- (void)signinSyncMediatorDidSuccessfulyFinishSigninForAdvancedSettings:
    (SigninSyncMediator*)signinSyncMediator;

// Called when revert the sign-in and sync operation if needed did successfully
// finish.
- (void)signinSyncMediatorDidSuccessfulyFinishSignout:
    (SigninSyncMediator*)signinSyncMediator;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_MEDIATOR_DELEGATE_H_
