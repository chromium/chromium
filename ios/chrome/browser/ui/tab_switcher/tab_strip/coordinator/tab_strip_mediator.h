// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_COORDINATOR_TAB_STRIP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_COORDINATOR_TAB_STRIP_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_mutator.h"

class Browser;
class BrowserList;
@protocol TabStripCommands;
@protocol TabStripConsumer;
class WebStateList;

namespace base {
class Uuid;
}
namespace tab_groups {
class TabGroupId;
class TabGroupSyncService;
class TabGroupVisualData;
}
namespace web {
class WebStateID;
}

// This mediator used to manage model interaction for its consumer.
@interface TabStripMediator
    : NSObject <TabCollectionDragDropHandler, TabStripMutator>

// The WebStateList that this mediator listens for any changes on the total
// number of Webstates.
@property(nonatomic, assign) WebStateList* webStateList;

// The ProfileIOS model for the corresponding browser.
@property(nonatomic, assign) ProfileIOS* profile;

// The associated browser needed to move tabs across browsers.
@property(nonatomic, assign) Browser* browser;

// Commands handler for the Tab Strip.
@property(nonatomic, weak) id<TabStripCommands> tabStripHandler;

// Designated initializer. Initializer with a TabStripConsumer, a
// `tabGroupSyncService` and the `browserList`.
- (instancetype)initWithConsumer:(id<TabStripConsumer>)consumer
             tabGroupSyncService:
                 (tab_groups::TabGroupSyncService*)tabGroupSyncService
                     browserList:(BrowserList*)browserList
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

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_COORDINATOR_TAB_STRIP_MEDIATOR_H_
