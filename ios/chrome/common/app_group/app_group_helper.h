// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_HELPER_H_
#define IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_HELPER_H_

#import <Foundation/Foundation.h>

// A pure Objective-C interface for app group functionality that can be
// included in Swift extensions via bridging headers.
@interface AppGroupHelper : NSObject

// Gets the application group.
+ (NSString*)applicationGroup;

// Returns an autoreleased pointer to the shared user defaults if an
// application group is defined for the application and its extensions.
// If not (i.e. on simulator, or if entitlements do not allow it) returns
// [NSUserDefaults standardUserDefaults].
+ (NSUserDefaults*)groupUserDefaults;

// Directory containing the favicons to be used by widgets.
+ (NSURL*)widgetsFaviconsFolder;

@end

#endif  // IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_HELPER_H_
