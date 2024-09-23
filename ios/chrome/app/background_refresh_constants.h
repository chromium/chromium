// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_REFRESH_CONSTANTS_H_
#define IOS_CHROME_APP_BACKGROUND_REFRESH_CONSTANTS_H_

#import <Foundation/Foundation.h>

// The identifier used to register and schedule background feed refresh tasks.
extern NSString* const kFeedBackgroundRefreshTaskIdentifier;

// NSUserDefaults key for the last time background refresh was called.
extern NSString* const kFeedLastBackgroundRefreshTimestamp;

// The identifier used to register and schedule background app refresh tasks.
extern NSString* const kAppBackgroundRefreshTaskIdentifier;

#endif  // IOS_CHROME_APP_BACKGROUND_REFRESH_CONSTANTS_H_
