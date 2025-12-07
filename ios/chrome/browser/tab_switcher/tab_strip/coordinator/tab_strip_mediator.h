// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_STRIP_COORDINATOR_TAB_STRIP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_STRIP_COORDINATOR_TAB_STRIP_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/tab_switcher/tab_strip/ui/tab_strip_group_cell_data_source.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/ui/tab_strip_mutator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item_snapshot_and_favicon_data_source.h"

class Browser;
class BrowserList;
class FaviconLoader;
class ProfileIOS;
class ShareKitService;
enum class TabGroupActionType;
@protocol TabStripCommands;
@protocol TabStripConsumer;
@protocol TabStripMediatorDelegate;
class UrlLoadingBrowserAgent;

namespace base {
class Uuid;
}  // namespace base

namespace collaboration {
class CollaborationService;
namespace messaging {
class MessagingBackendService;
}  // namespace messaging
}  // namespace collaboration

namespace tab_groups {
class TabGroupId;
class TabGroupSyncService;
class TabGroupVisualData;
}  // namespace tab_groups

namespace web {
class WebStateID;
}  // namespace web

// This mediator used to manage model interaction for its consumer.
@interface TabStripMediator
    : NSObject <TabCollectionDragDropHandler,
                TabSwitcherItemSnapShotAndFaviconDataSource,
                TabStripMutator,
                TabStripTabGroupCellDataSource>

// The ProfileIOS model for the corresponding browser.
@property(nonatomic, assign) ProfileIOS* profile;

// The associated browser needed to move tabs across browsers.
@property(nonatomic, assign) Browser* browser;

// Commands handler for the Tab Strip.
@property(nonatomic, weak) id<TabStripCommands> tabStripHandler;

// URL loader to open tabs when needed.
@property(nonatomic, assign) UrlLoadingBrowserAgent* URLLoader;

// Delegate.
@property(nonatomic, weak) id<TabStripMediatorDelegate> delegate;

// Designated initializer. Initializer with a TabStripConsumer, a
// `tabGroupSyncService` and the `browserList`.
- (instancetype)
        initWithConsumer:(id<TabStripConsumer>)consumer
     tabGroupSyncService:(tab_groups::TabGroupSyncService*)tabGroupSyncService
             browserList:(BrowserList*)browserList
        messagingService:
            (collaboration::messaging::MessagingBackendService*)messagingService
         shareKitService:(ShareKitService*)shareKitService
    collaborationService:
        (collaboration::CollaborationService*)collaborationService
           faviconLoader:(FaviconLoader*)faviconLoader
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Preprares the receiver for destruction, disconnecting from all services.
// It is an error for the receiver to dealloc without this having been called
// first.
- (void)disconnect;

// Cancels the move the `tabID` by moving it back to its `originBrowser` and
// `originIndex` and creates a new group based on `visualData`.
- (void)cancelMoveForTab:(web::WebStateID)tabID
           originBrowser:(Browser*)originBrowser
             originIndex:(int)originIndex
              visualData:(const tab_groups::TabGroupVisualData&)visualData
            localGroupID:(const tab_groups::TabGroupId&)localGroupID
                 savedID:(const base::Uuid&)savedID;

// Deletes the saved group with `savedID`.
- (void)deleteSavedGroupWithID:(const base::Uuid&)savedID;

// Ungroups all tabs in `tabGroupItem`. The tabs in the group remain open.
- (void)ungroupGroup:(TabGroupItem*)tabGroupItem;

// Closes and deletes all tabs in `tabGroupItem`.
- (void)deleteGroup:(TabGroupItem*)tabGroupItem;

// Completes the final removal of the last tab from its shared group. The last
// tab is temporarily saved until this function is triggered upon user
// confirmation.
- (void)closeSavedTabFromGroup:(TabGroupItem*)tabGroupItem;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_STRIP_COORDINATOR_TAB_STRIP_MEDIATOR_H_
