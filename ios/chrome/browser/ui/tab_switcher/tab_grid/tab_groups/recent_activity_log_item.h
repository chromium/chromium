// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_ITEM_H_

#import <UIKit/UIKit.h>

// Different type used for RecentActivityLogItem.
enum class ActivityLogType : NSUInteger {
  kTabAdded,
  kTabRemoved,
  kTabNavigated,
  kUserLeft,
  kGroupColorChanged,
  kGroupNameChanged,
};

// Represents a log item in a diffable data source. It contains the data of
// ActivityLogItem obtained from MessagingBackendService.
@interface RecentActivityLogItem : NSObject

// The type of the activity log.
@property(nonatomic, readonly) ActivityLogType type;

// TODO(crbug.com/370897655): Store an ID of the ActivityLogItem struct.

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_ITEM_H_
