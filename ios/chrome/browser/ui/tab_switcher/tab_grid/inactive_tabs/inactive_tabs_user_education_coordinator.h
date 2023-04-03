// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_USER_EDUCATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_USER_EDUCATION_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class InactiveTabsUserEducationCoordinator;

// Delegate for the coordinator.
@protocol InactiveTabsUserEducationCoordinatorDelegate

// Tells the delegate that the user tapped the Go to Settings button.
- (void)inactiveTabsUserEducationCoordinatorDidTapSettingsButton:
    (InactiveTabsUserEducationCoordinator*)inactiveTabsUserEducationCoordinator;

// Tells the delegate that the user closed the user education screen, either by
// tapping the Done button, or by swiping it away.
- (void)inactiveTabsUserEducationCoordinatorDidFinish:
    (InactiveTabsUserEducationCoordinator*)inactiveTabsUserEducationCoordinator;

@end

// Handles showing the user education screen for Inactive Tabs.
@interface InactiveTabsUserEducationCoordinator : ChromeCoordinator

// Delegate for dismissing the coordinator.
@property(nonatomic, weak) id<InactiveTabsUserEducationCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_USER_EDUCATION_COORDINATOR_H_
