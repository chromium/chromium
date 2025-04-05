// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_ITEM_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_ITEM_H_

#import <UIKit/UIKit.h>

namespace collaboration::messaging {
struct MessageAttribution;
enum class RecentActivityAction;
}  // namespace collaboration::messaging

@protocol ShareKitAvatarPrimitive;

// Represents a log item in a diffable data source. It contains the data of
// ActivityLogItem obtained from MessagingBackendService.
@interface RecentActivityLogItem : NSObject

// TODO(crbug.com/370897655): Store an ID of the ActivityLogItem struct.

// When true, all other values should be ignored. This represents an absence of
// item.
@property(nonatomic, assign) BOOL emptyItem;

// The image of a favicon of a page.
@property(nonatomic, strong) UIImage* favicon;

// The object to provide an avatar image.
@property(nonatomic, strong) id<ShareKitAvatarPrimitive> avatarPrimitive;

// The string of a title.
@property(nonatomic, strong) NSString* title;

// The string of a description.
@property(nonatomic, strong) NSString* actionDescription;

// The string of the timestamp when an action is taken.
@property(nonatomic, strong) NSString* timestamp;

// The type of action to be taken when this activity row is clicked.
// Not to be used by the UI.
@property(nonatomic, assign)
    collaboration::messaging::RecentActivityAction action;

// Implicit metadata that will be used to invoke the delegate when the
// activity row is clicked.
// Not to be used by the UI.
@property(nonatomic, assign)
    collaboration::messaging::MessageAttribution activityMetadata;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_ITEM_H_
