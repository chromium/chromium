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
  kTabUpdated,
  kMemberRemoved,
  kGroupColorChanged,
  kGroupNameChanged,
  kUndefined,
};

// Represents a log item in a diffable data source. It contains the data of
// ActivityLogItem obtained from MessagingBackendService.
@interface RecentActivityLogItem : NSObject

// TODO(crbug.com/370897655): Store an ID of the ActivityLogItem struct.

// The type of the activity log.
@property(nonatomic, assign) ActivityLogType type;

// The image of a favicon of a page.
@property(nonatomic, strong) UIImage* favicon;

// The image of a user icon.
@property(nonatomic, strong) UIImage* userIcon;

// The string of a title.
@property(nonatomic, strong) NSString* title;

// The string of a description.
@property(nonatomic, strong) NSString* actionDescription;

// The string of the timestamp when an action is taken.
@property(nonatomic, strong) NSString* timestamp;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_ITEM_H_
