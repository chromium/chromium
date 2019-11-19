// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_TO_CHROME_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_TO_CHROME_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
class Browser;
@protocol SyncPresenter;

// The first screen displayed to the user on First Run. User must agree to the
// Chrome Terms of Service before proceeding to use Chrome.
//
// Note: On iPhone, this controller supports portrait orientation only. It
// should always be presented in an |OrientationLimitingNavigationController|.
@interface WelcomeToChromeViewController : UIViewController

// True when the stats checkbox should be checked by default.
+ (BOOL)defaultStatsCheckboxValue;

// Initializes with the given browser state object and tab model, neither of
// which can be nil.
- (instancetype)initWithBrowser:(Browser*)browser
                      presenter:(id<SyncPresenter>)presenter
                     dispatcher:(id<ApplicationCommands>)dispatcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_TO_CHROME_VIEW_CONTROLLER_H_
