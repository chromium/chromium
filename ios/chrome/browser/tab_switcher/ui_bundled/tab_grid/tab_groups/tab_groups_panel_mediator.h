// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_page_mutator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_item_data_source.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_mutator.h"

namespace base {
class Uuid;
}  // namespace base

namespace collaboration {
class CollaborationService;
namespace messaging {
class MessagingBackendService;
}  // namespace messaging
}  // namespace collaboration

namespace data_sharing {
class DataSharingService;
}  // namespace data_sharing

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

@protocol ApplicationCommands;
class BrowserList;
class FaviconLoader;
@protocol GridToolbarsMutator;
class ShareKitService;
@protocol TabGridCommands;
@protocol TabGroupsPanelConsumer;
@protocol TabGroupsPanelMediatorDelegate;
class WebStateList;
@protocol TabGroupsCommands;

// TabGroupsPanelMediator controls the Tab Groups panel in Tab Grid.
@interface TabGroupsPanelMediator : NSObject <TabGridPageMutator,
                                              TabGroupsPanelItemDataSource,
                                              TabGroupsPanelMutator>

// The UI consumer to which updates are made.
@property(nonatomic, weak) id<TabGroupsPanelConsumer> consumer;

// Delegate.
@property(nonatomic, weak) id<TabGroupsPanelMediatorDelegate> delegate;

// Mutator to handle toolbars modification.
@property(nonatomic, weak) id<GridToolbarsMutator> toolbarsMutator;

// Tab Grid handler.
@property(nonatomic, weak) id<TabGridCommands> tabGridHandler;

// Application commands handler.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;

// Tab Groups command handler.
@property(nonatomic, weak) id<TabGroupsCommands> tabGroupsCommands;

// - `tabGroupSyncService`: the data source for the Tab Groups panel.
// - `regularWebStateList`: used to configure the Done button. Must not be null.
// - `disabled`: tells the mediator whether the Tab Groups panel is disabled, to
//     configure the toolbars.
- (instancetype)
    initWithTabGroupSyncService:
        (tab_groups::TabGroupSyncService*)tabGroupSyncService
                shareKitService:(ShareKitService*)shareKitService
           collaborationService:
               (collaboration::CollaborationService*)collaborationService
               messagingService:
                   (collaboration::messaging::MessagingBackendService*)
                       messagingService
             dataSharingService:
                 (data_sharing::DataSharingService*)dataSharingService
            regularWebStateList:(WebStateList*)regularWebStateList
                  faviconLoader:(FaviconLoader*)faviconLoader
               disabledByPolicy:(BOOL)disabled
                    browserList:(BrowserList*)browserList
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Deletes a synced group for `syncID`.
- (void)deleteSyncedTabGroup:(const base::Uuid&)syncID;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MEDIATOR_H_
