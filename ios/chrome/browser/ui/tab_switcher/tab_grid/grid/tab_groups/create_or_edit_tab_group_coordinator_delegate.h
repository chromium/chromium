// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_CREATE_OR_EDIT_TAB_GROUP_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_CREATE_OR_EDIT_TAB_GROUP_COORDINATOR_DELEGATE_H_

@class CreateTabGroupCoordinator;

// Delegate protocol for `CreateOrEditTabGroupCoordinator`.
@protocol CreateOrEditTabGroupCoordinatorDelegate

// Called when the user dismissed the `CreateOrEditTabGroupCoordinator`.
- (void)createOrEditTabGroupCoordinatorDidDismiss:
    (CreateTabGroupCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_CREATE_OR_EDIT_TAB_GROUP_COORDINATOR_DELEGATE_H_
