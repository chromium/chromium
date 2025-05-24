// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_CONSTANTS_H_

#import <Foundation/Foundation.h>

// The different swipe types.
enum class SwipeType { NONE, CHANGE_TAB, CHANGE_PAGE };

// Navigation Directions.
typedef NS_ENUM(NSInteger, NavigationDirection) {
  NavigationDirectionBack,
  NavigationDirectionForward
};

// Notification sent when the user starts a side swipe (on iPad).
NSString* const kSideSwipeWillStartNotification =
    @"kSideSwipeWillStartNotification";
// Notification sent when the user finishes a side swipe (on iPad).
NSString* const kSideSwipeDidStopNotification =
    @"kSideSwipeDidStopNotification";

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_CONSTANTS_H_
