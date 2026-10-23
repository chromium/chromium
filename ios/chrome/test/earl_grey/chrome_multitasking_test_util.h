// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_MULTITASKING_TEST_UTIL_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_MULTITASKING_TEST_UTIL_H_

#import <Foundation/Foundation.h>

// Reusable utility class containing gestures and checks to manipulate
// iPad multitasking windows and Stage Manager mode.
// Note: This utility has been designed to work on iPad running iOS 26+.
@interface ChromeMultitaskingTestUtil : NSObject

// Drags the bottom-right corner of the window inwards to put the app in
// windowed mode (Stage Manager). Asserts that the window successfully enters
// windowed mode.
+ (void)moveToWindowedMode;

// Drags the bottom-left corner of the window to transition the app to compact
// width. Asserts that the window successfully transitions to compact width.
+ (void)resizeWindowToCompact;

// Drags the bottom-left corner of the window back towards the left edge to
// restore regular width size class. Asserts that the window successfully
// transitions to regular width.
+ (void)resizeWindowToRegular;

// Drags the bottom-right corner of the window to the bottom-right of the screen
// to restore the app to fullscreen mode. Asserts that the window successfully
// exits windowed mode.
+ (void)moveToFullscreenMode;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_MULTITASKING_TEST_UTIL_H_
