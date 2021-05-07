// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/first_run/first_run_screen_view_controller.h"

// Extends the base delegate protocol to handle taps on the custom button.
@protocol
    WelcomeScreenViewControllerDelegate <FirstRunScreenViewControllerDelegate>

// Called when the user taps to see the terms and services page.
- (void)didTapTOSLink;

// Returns whether the metrics reporting consent checkbox should be selected or
// not by default.
- (BOOL)isCheckboxSelectedByDefault;

@end

// View controller of welcome screen.
@interface WelcomeScreenViewController : FirstRunScreenViewController

@property(nonatomic, weak) id<WelcomeScreenViewControllerDelegate> delegate;

// Whether the metrics reporting check box is selected.
@property(nonatomic, readonly, assign) BOOL checkBoxSelected;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_VIEW_CONTROLLER_H_
