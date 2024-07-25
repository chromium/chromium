// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MEDIATOR_DELEGATE_H_

#import "base/uuid.h"

@class TabGroupsPanelMediator;

// Delegate protocol for the tab group panel mediator.
@protocol TabGroupsPanelMediatorDelegate

// Opens the group with `syncID`.
- (void)tabGroupsPanelMediator:(TabGroupsPanelMediator*)tabGroupsPanelMediator
           openGroupWithSyncID:(const base::Uuid&)syncID;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MEDIATOR_DELEGATE_H_
