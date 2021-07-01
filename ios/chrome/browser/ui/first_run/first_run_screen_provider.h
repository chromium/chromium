// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/first_run/first_run_screen_type.h"

// The class that provides a list of first run screens.
@interface FirstRunScreenProvider : NSObject

- (instancetype)init NS_DESIGNATED_INITIALIZER;

// Returns the screen type of next screen.
- (FirstRunScreenType)nextScreenType;

// Removes sync screen if the user skipped sign in.
- (void)userSkippedSignIn;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_PROVIDER_H_
