// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/first_run/first_run_screen_view_controller.h"

@protocol TOSCommands;

// Extends the base delegate protocol to handle taps on the custom button.
@protocol
    WelcomeScreenViewControllerDelegate <FirstRunScreenViewControllerDelegate>

// Returns whether the metrics reporting consent checkbox should be selected or
// not by default.
- (BOOL)isCheckboxSelectedByDefault;

@end

// View controller of welcome screen.
@interface WelcomeScreenViewController : FirstRunScreenViewController

@property(nonatomic, weak) id<WelcomeScreenViewControllerDelegate> delegate;

// Whether the metrics reporting checkbox is selected.
@property(nonatomic, readonly, assign) BOOL checkBoxSelected;

// Init with the handler used to manage the display of TOS.
- (instancetype)initWithTOSHandler:(id<TOSCommands>)TOSHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_VIEW_CONTROLLER_H_
