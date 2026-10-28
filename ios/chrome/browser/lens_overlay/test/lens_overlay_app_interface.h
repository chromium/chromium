// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_TEST_LENS_OVERLAY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_TEST_LENS_OVERLAY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Test app interface for Lens Overlay and Lens View Finder.
@interface LensOverlayAppInterface : NSObject

// Presents the Lens View Finder via `LensCommands` in the window with the given
// number and waits until its view is added to the window hierarchy. Returns YES
// on success.
+ (BOOL)presentLensViewFinderInWindowWithNumber:(int)windowNumber [[nodiscard]];

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_TEST_LENS_OVERLAY_APP_INTERFACE_H_
