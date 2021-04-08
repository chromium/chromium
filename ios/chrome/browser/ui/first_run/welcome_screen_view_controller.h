// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Delegate of welcome screen view controller.
@protocol WelcomeScreenViewControllerDelegate <NSObject>

// Called when the user taps on the "Terms of Service" link.
- (void)didTapTOSLink;

// Called when the user taps the "Accept & Continue" button.
- (void)didTapContinueButton;

@end

// View controller of welcome screen.
// TODO(crbug.com/1189815): conform the shared ScreenViewController when
// crbug/1186762 is done.
@interface WelcomeScreenViewController : UIViewController

@property(nonatomic, weak) id<WelcomeScreenViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_SCREEN_VIEW_CONTROLLER_H_
