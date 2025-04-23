// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_FULLSCREEN_SIGNIN_SCREEN_UI_FULLSCREEN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_FULLSCREEN_SIGNIN_SCREEN_UI_FULLSCREEN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin_screen/ui/fullscreen_signin_screen_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

enum class SigninContextStyle;
@protocol TOSCommands;

// Delegate for the fullscreen sign-in view controller.
@protocol FullscreenSigninScreenViewControllerDelegate <
    PromoStyleViewControllerDelegate>

// Called when the user taps to see the account picker.
- (void)showAccountPickerFromPoint:(CGPoint)point;

@end

// View controller for a fullscreen sign-in.
@interface FullscreenSigninScreenViewController
    : PromoStyleViewController <FullscreenSigninScreenConsumer>

// Handler to open the terms of service dialog.
@property(nonatomic, weak) id<TOSCommands> TOSHandler;
@property(nonatomic, weak) id<FullscreenSigninScreenViewControllerDelegate>
    delegate;

// Designated initializer.
// The `contextStyle` is used to customize content on screen.
- (instancetype)initWithContextStyle:(SigninContextStyle)contextStyle
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_FULLSCREEN_SIGNIN_SCREEN_UI_FULLSCREEN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_
