// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/first_run/signin/signin_screen_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// Delegate of sign-in screen view controller.
@protocol SigninScreenViewControllerDelegate <PromoStyleViewControllerDelegate>

// TODO(crbug.com/1290848): Need implementation.

@end

// View controller of sign-in screen.
@interface SigninScreenViewController
    : PromoStyleViewController <SigninScreenConsumer>

@property(nonatomic, weak) id<SigninScreenViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_
