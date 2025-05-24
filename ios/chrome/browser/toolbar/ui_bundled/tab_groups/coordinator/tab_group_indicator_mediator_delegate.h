// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"

class TabGroup;
enum class TabGroupActionType;
namespace collaboration {
enum class CollaborationServiceShareOrManageEntryPoint;
}  // namespace collaboration

// Delegate for actions happening in the TabGroupIndicatorMediator.
@protocol TabGroupIndicatorMediatorDelegate

// Shows tab group editing view.
- (void)showTabGroupIndicatorEditionForGroup:
    (base::WeakPtr<const TabGroup>)tabGroup;

// Shows the recent activity for the shared group.
- (void)showRecentActivityForGroup:(base::WeakPtr<const TabGroup>)tabGroup;

// Displays a confirmation dialog to confirm that the current `tabGroup` is
// going to take an `actionType`.
- (void)
    showTabGroupIndicatorConfirmationForAction:(TabGroupActionType)actionType
                                         group:(base::WeakPtr<const TabGroup>)
                                                   tabGroup;

// Starts the leave or delete shared group flow.
- (void)startLeaveOrDeleteSharedGroup:(base::WeakPtr<const TabGroup>)tabGroup
                            forAction:(TabGroupActionType)actionType;

// Displays a snackbar after closing a tab group locally.
- (void)showTabGroupIndicatorSnackbarAfterClosingGroup;

// Displays the IPH when the user foreground the app while a shared tab group is
// active.
- (void)showIPHForSharedTabGroupForegrounded;

// Shows the "share" or "manage" screen for the `tabGroup`. The choice is
// automatically made based on whether the group is already shared or not.
- (void)
    shareOrManageTabGroup:(const TabGroup*)tabGroup
               entryPoint:
                   (collaboration::CollaborationServiceShareOrManageEntryPoint)
                       entryPoint;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_DELEGATE_H_
