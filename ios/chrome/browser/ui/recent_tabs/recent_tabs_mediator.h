// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions_bridge.h"
#import "ios/chrome/browser/ui/recent_tabs/closed_tabs_observer_bridge.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_activity_observer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_page_mutator.h"

class BrowserList;
class FaviconLoader;
@protocol GridConsumer;
@protocol GridToolbarsMutator;
@protocol RecentTabsConsumer;
@class SceneState;
@protocol TabGridCommands;
@class TabGridModeHolder;

namespace feature_engagement {
class Tracker;
}

namespace signin {
class IdentityManager;
}  // namespace signin

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

namespace syncer {
class SyncService;
}  // namespace syncer

namespace sessions {
class TabRestoreService;
}  // namespace sessions

// RecentTabsMediator controls the RecentTabsConsumer, based on the user's
// signed-in and chrome-sync states.
//
// RecentTabsMediator listens for notifications about Chrome Sync and
// ChromeToDevice and changes/updates the RecentTabsConsumer accordingly.
@interface RecentTabsMediator : NSObject <ClosedTabsObserving,
                                          RecentTabsTableViewControllerDelegate,
                                          TabGridActivityObserver,
                                          TabGridPageMutator,
                                          TableViewFaviconDataSource>

// The consumer for this object. This can change during the lifetime of this
// object and may be nil.
@property(nonatomic, strong) id<RecentTabsConsumer> consumer;
// Mutator to handle toolbars modification.
@property(nonatomic, weak) id<GridToolbarsMutator> toolbarsMutator;
// Grid consumer.
@property(nonatomic, weak) id<GridConsumer> gridConsumer;
// Handler for the Tab Grid commands.
@property(nonatomic, weak) id<TabGridCommands> tabGridHandler;

- (instancetype)
    initWithSessionSyncService:
        (sync_sessions::SessionSyncService*)sessionSyncService
               identityManager:(signin::IdentityManager*)identityManager
                restoreService:(sessions::TabRestoreService*)restoreService
                 faviconLoader:(FaviconLoader*)faviconLoader
                   syncService:(syncer::SyncService*)syncService
                   browserList:(BrowserList*)browserList
                    sceneState:(SceneState*)sceneState
              disabledByPolicy:(BOOL)disabled
             engagementTracker:(feature_engagement::Tracker*)engagementTracker
                    modeHolder:(TabGridModeHolder*)modeHolder
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts observing the he user's signed-in and chrome-sync states.
- (void)initObservers;

// Disconnects the mediator from all observers.
- (void)disconnect;

// Configures the consumer with current data. Intended to be called immediately
// after initialization.
- (void)configureConsumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MEDIATOR_H_
