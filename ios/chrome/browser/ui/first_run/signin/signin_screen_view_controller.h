// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/first_run/signin/signin_screen_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@protocol TOSCommands;

// Delegate of sign-in screen view controller.
@protocol SigninScreenViewControllerDelegate <PromoStyleViewControllerDelegate>

// Called when the user taps to see the account picker.
- (void)showAccountPickerFromPoint:(CGPoint)point;

// Logs scrollability metric on view appears.
- (void)logScrollButtonVisible:(BOOL)scrollButtonVisible
            withIdentityPicker:(BOOL)identityPickerVisible
                     andFooter:(BOOL)footerVisible;

@end

// View controller of sign-in screen.
@interface SigninScreenViewController
    : PromoStyleViewController <SigninScreenConsumer>

// Handler to open the terms of service dialog.
@property(nonatomic, weak) id<TOSCommands> TOSHandler;
@property(nonatomic, weak) id<SigninScreenViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_
