// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_BACKGROUND_LAUNCH_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_BACKGROUND_LAUNCH_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App interface for simulating background launches and lifecycle transitions in
// EarlGrey tests.
@interface BackgroundLaunchAppInterface : NSObject

// Resumes the app startup sequence and transitions connected scenes to
// foreground active. The intent is to simulate the user opening the app
// after it was launched in the background.
+ (void)unblockStartupAndForeground;

// Simulates a background refresh task launch.
+ (void)simulateBackgroundRefresh;

// Simulates a background URL session event launch.
+ (void)simulateBackgroundURLSession;

// Simulates a background silent remote notification launch.
+ (void)simulateBackgroundSilentNotification;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_BACKGROUND_LAUNCH_APP_INTERFACE_H_
