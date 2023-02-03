// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_CHROME_OVERLAY_WINDOW_H_
#define IOS_CHROME_APP_CHROME_OVERLAY_WINDOW_H_

#import <MaterialComponents/MaterialOverlayWindow.h>

// Tracks size classes changes then reports to SizeClassRecorder and Crash keys.
@interface ChromeOverlayWindow : MDCOverlayWindow
@end

#endif  // IOS_CHROME_APP_CHROME_OVERLAY_WINDOW_H_
