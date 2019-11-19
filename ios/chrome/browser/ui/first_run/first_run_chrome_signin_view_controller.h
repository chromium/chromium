// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_CHROME_SIGNIN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_CHROME_SIGNIN_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"

@protocol ApplicationCommands;
class Browser;
@class FirstRunConfiguration;
@protocol SyncPresenter;

// A ChromeSigninViewController that is used during the run.
@interface FirstRunChromeSigninViewController : ChromeSigninViewController

// Designated initialzer.
- (instancetype)initWithBrowser:(Browser*)browser
                 firstRunConfig:(FirstRunConfiguration*)firstRunConfig
                 signInIdentity:(ChromeIdentity*)identity
                      presenter:(id<SyncPresenter>)presenter
                     dispatcher:(id<ApplicationCommands>)dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_CHROME_SIGNIN_VIEW_CONTROLLER_H_
