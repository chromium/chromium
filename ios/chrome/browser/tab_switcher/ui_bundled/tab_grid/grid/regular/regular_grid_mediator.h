// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_REGULAR_REGULAR_GRID_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_REGULAR_REGULAR_GRID_MEDIATOR_H_

#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/coordinator/base_grid_mediator.h"

namespace collaboration::messaging {
class MessagingBackendService;
}  // namespace collaboration::messaging

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

@protocol RegularGridMediatorDelegate;
class ShareKitService;

// Mediates between model layer and regular grid UI layer.
@interface RegularGridMediator : BaseGridMediator

// Sends updates from the regular model layer to the inactive tabs model layer.
// This is needed, for example, when a user close all tabs from the regular grid
// as it also close all inactives tabs.
// TODO(crbug.com/40273478): Refactor these to be a mutator.
@property(nonatomic, weak) id<GridCommands> inactiveTabsGridCommands;

// Regular delegate.
@property(nonatomic, weak) id<RegularGridMediatorDelegate> regularDelegate;

// Designated initialized. `tabGroupSyncService`, `shareKitService` and
// `messagingService`: can be `nullptr`.
- (instancetype)
     initWithModeHolder:(TabGridModeHolder*)modeHolder
    tabGroupSyncService:(tab_groups::TabGroupSyncService*)tabGroupSyncService
        shareKitService:(ShareKitService*)shareKitService
       messagingService:
           (collaboration::messaging::MessagingBackendService*)messagingService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithModeHolder:(TabGridModeHolder*)modeHolder
    NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_REGULAR_REGULAR_GRID_MEDIATOR_H_
