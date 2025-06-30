// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_H_

#import <Foundation/Foundation.h>

#import "base/uuid.h"
#import "ios/chrome/browser/share_kit/model/sharing_state.h"

// Different types of items identified by a TabGroupsPanelItem.
enum class TabGroupsPanelItemType : NSUInteger {
  kOutOfDateMessage,
  kNotification,
  kSavedTabGroup,
};

// Identifies an entry in the Tab Groups panel.
@interface TabGroupsPanelItem : NSObject

// The type of the item.
@property(nonatomic, readonly) TabGroupsPanelItemType type;

// The text of the notification, when `type` is `kNotification`.
@property(nonatomic, readonly) NSString* notificationText;

// The saved group's ID, when `type` is `kSavedTabGroup`.
@property(nonatomic, readonly) base::Uuid savedTabGroupID;

// The sharing state of the item, when type is `kSavedTabGroup`.
@property(nonatomic, readonly) tab_groups::SharingState sharingState;

- (instancetype)initWithOutOfDateMessage NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNotificationText:(NSString*)text
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithSavedTabGroupID:(base::Uuid)savedTabGroupID
                           sharingState:(tab_groups::SharingState)sharingState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_H_
