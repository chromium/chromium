// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_SHARED_TAB_GROUP_USER_EDUCATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_SHARED_TAB_GROUP_USER_EDUCATION_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class SharedTabGroupUserEducationCoordinator;

// Delegate for the coordinator.
@protocol SharedTabGroupUserEducationCoordinatorDelegate

// Called when the user education screen got dismissed.
- (void)userEducationCoordinatorDidDismiss:
    (SharedTabGroupUserEducationCoordinator*)coordinator;

@end

// The coordinator to display a half sheet user education.
@interface SharedTabGroupUserEducationCoordinator : ChromeCoordinator

// The delegate for this coordinator.
@property(nonatomic, weak) id<SharedTabGroupUserEducationCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_SHARED_TAB_GROUP_USER_EDUCATION_COORDINATOR_H_
