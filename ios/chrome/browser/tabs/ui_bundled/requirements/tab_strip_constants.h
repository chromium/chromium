// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_REQUIREMENTS_TAB_STRIP_CONSTANTS_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_REQUIREMENTS_TAB_STRIP_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Enum defining the different styles for the tab strip.
typedef NS_ENUM(NSUInteger, TabStripStyle) { NORMAL, INCOGNITO };

// Notification when the tab strip will start an animation.
extern NSString* const kWillStartTabStripTabAnimation;

// Notifications when the user starts and ends a drag operation.
extern NSString* const kTabStripDragStarted;
extern NSString* const kTabStripDragEnded;

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_REQUIREMENTS_TAB_STRIP_CONSTANTS_H_
