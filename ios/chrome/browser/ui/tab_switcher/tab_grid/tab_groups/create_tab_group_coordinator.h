// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_CREATE_TAB_GROUP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_CREATE_TAB_GROUP_COORDINATOR_H_

#import <set>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol CreateOrEditTabGroupCoordinatorDelegate;
class TabGroup;
namespace web {
class WebStateID;
}

// Coordinator that manage the model and UI of the tab group creation.
@interface CreateTabGroupCoordinator : ChromeCoordinator

// Delegate.
@property(nonatomic, weak) id<CreateOrEditTabGroupCoordinatorDelegate> delegate;

// Whether the dismissal is animated. Default YES.
@property(nonatomic, assign) BOOL animatedDismissal;

// Initializer when you create a new group.
// - `identifiers` is the list of selected tab to put in the group once created.
- (instancetype)
    initTabGroupCreationWithBaseViewController:(UIViewController*)viewController
                                       browser:(Browser*)browser
                                  selectedTabs:
                                      (const std::set<web::WebStateID>&)
                                          identifiers NS_DESIGNATED_INITIALIZER;

// Initializer when you edit an existing `tabGroup` passed in parameters.
// `tabGroup` should not be nil.
- (instancetype)
    initTabGroupEditionWithBaseViewController:(UIViewController*)viewController
                                      browser:(Browser*)browser
                                     tabGroup:(const TabGroup*)tabGroup
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_CREATE_TAB_GROUP_COORDINATOR_H_
