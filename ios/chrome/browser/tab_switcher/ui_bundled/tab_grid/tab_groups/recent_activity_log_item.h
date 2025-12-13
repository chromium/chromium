// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_ITEM_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_ITEM_H_

#import <UIKit/UIKit.h>

class GURL;
namespace collaboration::messaging {
struct MessageAttribution;
enum class RecentActivityAction;
}  // namespace collaboration::messaging

@class FaviconAttributes;
@protocol ShareKitAvatarPrimitive;

// Represents a log item in a diffable data source. It contains the data of
// ActivityLogItem obtained from MessagingBackendService.
// The equality between two objects is based on the id of the
// `activityMetadata`.
@interface RecentActivityLogItem : NSObject

// Attributes for the favicon.
@property(nonatomic, strong) FaviconAttributes* attributes;

// GURL of the favicon.
@property(nonatomic, assign) GURL faviconURL;

// Object that provides an avatar image.
@property(nonatomic, strong) id<ShareKitAvatarPrimitive> avatarPrimitive;

// Title of the item.
@property(nonatomic, copy) NSString* title;

// Description of the item.
@property(nonatomic, copy) NSString* actionDescription;

// Elapsed time since the action occurred (e.g., "6h ago", "just now").
@property(nonatomic, copy) NSString* elapsedTime;

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
