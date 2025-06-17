// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MEDIATOR_DELEGATE_H_

#import "base/uuid.h"

@protocol FacePileProviding;
enum class TabGroupActionType;
@class TabGroupsPanelMediator;

// Delegate protocol for the tab group panel mediator.
@protocol TabGroupsPanelMediatorDelegate

// Opens the group with `syncID`.
- (void)tabGroupsPanelMediator:(TabGroupsPanelMediator*)tabGroupsPanelMediator
           openGroupWithSyncID:(const base::Uuid&)syncID;

// Displays a confirmation dialog anchoring to `sourceView` on iPad or at the
// bottom on iPhone to confirm that the group with `syncID` is going to be
// deleted.
- (void)tabGroupsPanelMediator:(TabGroupsPanelMediator*)tabGroupsPanelMediator
    showDeleteGroupConfirmationWithSyncID:(const base::Uuid)syncID
                               sourceView:(UIView*)sourceView;

// Starts the leave or delete shared group flow Ahoring to `sourceView` on iPad
// or at the bottom on iPhone.
- (void)tabGroupsPanelMediator:(TabGroupsPanelMediator*)tabGroupsPanelMediator
    startLeaveOrDeleteSharedGroupWithSyncID:(const base::Uuid)syncID
                                 groupTitle:(NSString*)groupTitle
                                  forAction:(TabGroupActionType)actionType
                                 sourceView:(UIView*)sourceView;

// Returns a FacePile provider for `groupID`.
- (id<FacePileProviding>)facePileProviderForGroupID:(const std::string&)groupID;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MEDIATOR_DELEGATE_H_
