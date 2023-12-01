// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_COORDINATOR_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator_delegate.h"

class Browser;
@protocol GridCoordinatorAudience;
@class IncognitoGridViewController;
@class IncognitoGridMediator;
@protocol TabContextMenuDelegate;

@interface IncognitoGridCoordinator
    : BaseGridCoordinator <IncognitoGridMediatorDelegate>

// The Grid view controller.
// TODO(crbug.com/1457146): Make it private.
@property(nonatomic, readonly, strong)
    IncognitoGridViewController* gridViewController;
// Incognito grid mediator.
// TODO(crbug.com/1457146): Make it private.
@property(nonatomic, readonly, strong)
    IncognitoGridMediator* incognitoGridMediator;

// Audience for this coordinator.
@property(nonatomic, weak) id<GridCoordinatorAudience> audience;
// Tab Context Menu delegate.
// TODO(crbug.com/1457146): This protocol should be implemented by this object.
@property(nonatomic, weak) id<TabContextMenuDelegate> tabContextMenuDelegate;

// The incognito browser can be reset during the execution of the app.
- (void)setIncognitoBrowser:(Browser*)incognitoBrowser;

// Stops all child coordinators.
- (void)stopChildCoordinators;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_COORDINATOR_H_
