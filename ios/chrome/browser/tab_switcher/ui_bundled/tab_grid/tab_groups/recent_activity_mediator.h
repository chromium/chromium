// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_mutator.h"
#import "ios/web/public/web_state.h"

class FaviconLoader;
@protocol RecentActivityCommands;
@protocol RecentActivityConsumer;
class ShareKitService;
class TabGroup;
class WebStateList;

namespace collaboration::messaging {
class MessagingBackendService;
}  // namespace collaboration::messaging
namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

// A mediator to control the recent activity logs in a shared tab group.
@interface RecentActivityMediator
    : NSObject <RecentActivityMutator, TableViewFaviconDataSource>

// Consumer of the recent activity.
@property(nonatomic, weak) id<RecentActivityConsumer> consumer;

// Handler for the recent activity commands.
@property(nonatomic, weak) id<RecentActivityCommands> recentActivityHandler;

// Designated initializer.
- (instancetype)initWithTabGroup:(base::WeakPtr<const TabGroup>)tabGroup
                messagingService:
                    (collaboration::messaging::MessagingBackendService*)
                        messagingService
                   faviconLoader:(FaviconLoader*)faviconLoader
                     syncService:(tab_groups::TabGroupSyncService*)syncService
                 shareKitService:(ShareKitService*)shareKitService
                    webStateList:(WebStateList*)webStateList
          webStateCreationParams:
              (const web::WebState::CreateParams&)webStateCreationParams
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_MEDIATOR_H_
