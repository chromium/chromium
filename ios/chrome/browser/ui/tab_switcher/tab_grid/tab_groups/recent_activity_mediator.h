// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"

class FaviconLoader;
class TabGroup;
@protocol RecentActivityConsumer;
namespace collaboration::messaging {
class MessagingBackendService;
}  // namespace collaboration::messaging
namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

// A mediator to control the recent activity logs in a shared tab group.
@interface RecentActivityMediator : NSObject

// Consumer of the recent activity.
@property(nonatomic, weak) id<RecentActivityConsumer> consumer;

// Designated initializer.
- (instancetype)initWithtabGroup:(base::WeakPtr<const TabGroup>)tabGroup
                messagingService:
                    (collaboration::messaging::MessagingBackendService*)
                        messagingService
                   faviconLoader:(FaviconLoader*)faviconLoader
                     syncService:(tab_groups::TabGroupSyncService*)syncService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_MEDIATOR_H_
