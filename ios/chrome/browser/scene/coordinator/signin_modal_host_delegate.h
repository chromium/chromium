// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCENE_COORDINATOR_SIGNIN_MODAL_HOST_DELEGATE_H_
#define IOS_CHROME_BROWSER_SCENE_COORDINATOR_SIGNIN_MODAL_HOST_DELEGATE_H_

#import <UIKit/UIKit.h>

@class SigninModalHost;

// Provides `SigninModalHost` with the scene UI state it needs to present the
// sign-in UI, which is owned by the scene UI rather than by the host.
@protocol SigninModalHostDelegate <NSObject>

// Returns the view controller currently displayed by the scene, on which
// the sign-in UI is presented by default.
- (UIViewController*)activeViewControllerForSigninModalHost:
    (SigninModalHost*)signinModalHost;

// Returns whether the sign-in UI can be presented on top of the current tab.
- (BOOL)isTabAvailableToPresentViewControllerForSigninModalHost:
    (SigninModalHost*)signinModalHost;

@end

#endif  // IOS_CHROME_BROWSER_SCENE_COORDINATOR_SIGNIN_MODAL_HOST_DELEGATE_H_
