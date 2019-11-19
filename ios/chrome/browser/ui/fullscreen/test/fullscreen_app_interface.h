// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_FULLSCREEN_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_FULLSCREEN_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// FullscreenAppInterface contains the app-side implementation for helpers.
// These helpers are compiled into the app binary and can be called from either
// app or test code.
@interface FullscreenAppInterface : NSObject

// Whether or not the fullscreen provider has been initialized.
+ (BOOL)isFullscreenInitialized;

// Returns the current viewport insets for the visible web content view.
+ (UIEdgeInsets)currentViewportInsets;

@end

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_FULLSCREEN_APP_INTERFACE_H_
