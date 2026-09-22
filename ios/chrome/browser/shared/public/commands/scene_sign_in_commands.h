// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SCENE_SIGN_IN_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SCENE_SIGN_IN_COMMANDS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"

class GURL;
@class ShowSigninCommand;
@class UIViewController;

// Commands related to scene-level sign-in workflows and UI.
@protocol SceneSignInCommands <NSObject>

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the signin UI, presenting from `baseViewController`.
// DISCLAIMER: If possible, prefer calling `[SigninCoordinator
// signinCoordinatorWithCommand:browser:baseViewController]` instead.
// Keep ownership of the `SigninCoordinator` and start it explicitly.
- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController;

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the consistency promo UI that allows users to sign in to Chrome using
// the default accounts on the device.
// Redirects to `url` when the sign-in flow is complete.
- (void)showWebSigninPromoFromViewController:
            (UIViewController*)baseViewController
                                         URL:(const GURL&)url;

// Shows the fullscreen sign-in promo with a completion block that is called
// when the promo is dismissed.
- (void)showFullscreenSigninPromoWithCompletion:
    (SigninCoordinatorCompletionCallback)dismissalCompletion;

// Stops the sign-in coordinator actions and dismisses its views either with or
// without animation. Executes its signinCompletion.
- (void)stopSigninCoordinatorWithCompletionAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SCENE_SIGN_IN_COMMANDS_H_
