// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAFE_MODE_SAFE_MODE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_SAFE_MODE_SAFE_MODE_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// EarlGreyScopedBlockSwizzlerAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface SafeModeAppInterface : NSObject

// Presents the SafeModeViewController UI.
+ (void)presentSafeMode;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAFE_MODE_SAFE_MODE_APP_INTERFACE_H_
