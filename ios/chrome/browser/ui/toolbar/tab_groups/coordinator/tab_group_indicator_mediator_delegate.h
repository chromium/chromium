// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"

class TabGroup;
enum class TabGroupActionType;

// Delegate for actions happening in the TabGroupIndicatorMediator.
@protocol TabGroupIndicatorMediatorDelegate

// Shows tab group editing view.
- (void)showTabGroupIndicatorEditionForGroup:
    (base::WeakPtr<const TabGroup>)tabGroup;

// Displays a confirmation dialog to confirm that the current group is going to
// take an `actionType`.
- (void)showTabGroupIndicatorConfirmationForAction:
    (TabGroupActionType)actionType;

// Displays a snackbar after closing a tab group locally.
- (void)showTabGroupIndicatorSnackbarAfterClosingGroup;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_DELEGATE_H_
