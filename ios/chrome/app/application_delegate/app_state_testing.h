// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_TESTING_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_TESTING_H_

#import "ios/chrome/app/application_delegate/app_state.h"

@class SafeModeCoordinator;

// Exposes methods for testing.
@interface AppState (Testing)

@property(nonatomic, retain) SafeModeCoordinator* safeModeCoordinator;

- (instancetype)
initWithBrowserLauncher:(id<BrowserLauncher>)browserLauncher
     startupInformation:(id<StartupInformation>)startupInformation
    applicationDelegate:(MainApplicationDelegate*)applicationDelegate
                 window:(UIWindow*)window
          shouldOpenNTP:(BOOL)shouldOpenNTP;

- (void)disableReporting;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_TESTING_H_
