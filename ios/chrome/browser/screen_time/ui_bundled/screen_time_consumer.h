// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREEN_TIME_UI_BUNDLED_SCREEN_TIME_CONSUMER_H_
#define IOS_CHROME_BROWSER_SCREEN_TIME_UI_BUNDLED_SCREEN_TIME_CONSUMER_H_

#import <Foundation/Foundation.h>

// A protocol to update information reported to the ScreenTime system.
@protocol ScreenTimeConsumer
// Sets `URL` as the active URL reported to the ScreenTime system when the
// underlying web view is visible.
- (void)setURL:(NSURL*)URL;

// Disables usage recording in the ScreenTime system.
- (void)setSuppressUsageRecording:(BOOL)suppressUsageRecording;
@end

#endif  // IOS_CHROME_BROWSER_SCREEN_TIME_UI_BUNDLED_SCREEN_TIME_CONSUMER_H_
