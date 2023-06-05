// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_SPOTLIGHT_LOGGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_SPOTLIGHT_LOGGER_H_

#import <CoreSpotlight/CoreSpotlight.h>

/// A logger for Spotlight-related events and known spotlight items used for
/// debugging. Only instantiated when the spotlight debugging is turned on in
/// experimental settings. Keeps a log of all items donated to Spotlight.
/// Intended to be used on main thread only.
/// - Discussion
/// As of writing, CoreSpotlight API doesn't provide a way to query the
/// Spotlight index and is limited to adding/deleting items only. This makes it
/// hard to debug Spotlight-related features. This class is designed to keep a
/// copy of the app's indexed items, thus making debugging easier.
@interface SpotlightLogger : NSObject

// Use `sharedLogger` instead.
- (instancetype)init NS_UNAVAILABLE;

// Returns a shared logger if debugging features are enabled, nil otherwise.
+ (instancetype)sharedLogger;

/// Logs any spotlight error. This method is available even when spotlight
/// debugger isn't enabled. When spotlight debugger is enabled, it shows an
/// alert and logs the error to a file that can be found in the debug UI. When
/// it's disabled, the error is reported through UMA.
+ (void)logSpotlightError:(NSError*)error;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_SPOTLIGHT_LOGGER_H_
