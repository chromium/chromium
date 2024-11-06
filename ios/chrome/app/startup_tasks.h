// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_TASKS_H_
#define IOS_CHROME_APP_STARTUP_TASKS_H_

#import <Foundation/Foundation.h>

// Class handling all startup tasks.
@interface StartupTasks : NSObject

// Starts Omaha and, if first run, sets install time.  For official builds only.
- (void)initializeOmaha;
// Registers to receive UIApplicationWillResignActiveNotification.
- (void)registerForApplicationWillResignActiveNotification;
// Logs the number of Chrome Siri Shortcuts to UMA.
- (void)logSiriShortcuts;

@end

#endif  // IOS_CHROME_APP_STARTUP_TASKS_H_
