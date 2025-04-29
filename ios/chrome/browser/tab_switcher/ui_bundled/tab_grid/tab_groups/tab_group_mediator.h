// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/base_grid_mediator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_mutator.h"

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

class ShareKitService;
class TabGroup;
@protocol TabCollectionConsumer;
@class TabGridModeHolder;
@protocol TabGroupsCommands;
@protocol TabGroupConsumer;
@class TabGroupMediator;
class WebStateList;

// Tab group mediator in charge to handle model update for one group.
@interface TabGroupMediator : BaseGridMediator <TabGroupMutator>

- (instancetype)
    initWithWebStateList:(WebStateList*)webStateList
     tabGroupSyncService:(tab_groups::TabGroupSyncService*)tabGroupSyncService
         shareKitService:(ShareKitService*)shareKitService
    collaborationService:
        (collaboration::CollaborationService*)collaborationService
      dataSharingService:(data_sharing::DataSharingService*)dataSharingService
                tabGroup:(base::WeakPtr<const TabGroup>)tabGroup
                consumer:(id<TabGroupConsumer>)consumer
            gridConsumer:(id<TabCollectionConsumer>)gridConsumer
              modeHolder:(TabGridModeHolder*)modeHolder
        messagingService:(collaboration::messaging::MessagingBackendService*)
                             messagingService;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_MEDIATOR_H_
