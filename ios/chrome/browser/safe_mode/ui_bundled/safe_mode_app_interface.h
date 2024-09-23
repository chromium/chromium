// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_MODE_UI_BUNDLED_SAFE_MODE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_SAFE_MODE_UI_BUNDLED_SAFE_MODE_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// EarlGreyScopedBlockSwizzlerAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface SafeModeAppInterface : NSObject

// Presents the SafeModeViewController UI.
+ (void)presentSafeMode;

//  Set the failed startup attempt counter to `count`.
+ (void)setFailedStartupAttemptCount:(int)count;

@end

#endif  // IOS_CHROME_BROWSER_SAFE_MODE_UI_BUNDLED_SAFE_MODE_APP_INTERFACE_H_
