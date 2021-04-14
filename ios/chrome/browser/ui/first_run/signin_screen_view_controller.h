// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/first_run/signin_screen_consumer.h"

// Delegate of sign-in screen view controller.
@protocol SigninScreenViewControllerDelegate <NSObject>

// Called when the user taps to see the account picker.
- (void)showAccountPicker;

@end

// View controller of sign-in screen.
@interface SigninScreenViewController : UIViewController <SigninScreenConsumer>

@property(nonatomic, weak) id<SigninScreenViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_
