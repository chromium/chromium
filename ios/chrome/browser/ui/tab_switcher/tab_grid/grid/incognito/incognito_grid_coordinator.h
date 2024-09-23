// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_COORDINATOR_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator_delegate.h"

class Browser;
@protocol GridCommands;
@protocol GridCoordinatorAudience;
@class IncognitoGridViewController;
@class IncognitoGridMediator;
@protocol TabContextMenuDelegate;

@interface IncognitoGridCoordinator
    : BaseGridCoordinator <IncognitoGridMediatorDelegate>

// The command handler to handle commands related to this grid. This is exposed
// to make sure other can use it.
@property(nonatomic, weak, readonly) id<GridCommands> gridHandler;

// The Grid view controller.
// TODO(crbug.com/40273478): Make it private.
@property(nonatomic, readonly, strong)
    IncognitoGridViewController* gridViewController;
// Incognito grid mediator.
// TODO(crbug.com/40273478): Make it private.
@property(nonatomic, readonly, strong)
    IncognitoGridMediator* incognitoGridMediator;

// Audience for this coordinator.
@property(nonatomic, weak) id<GridCoordinatorAudience> audience;

// The incognito browser can be reset during the execution of the app.
- (void)setIncognitoBrowser:(Browser*)incognitoBrowser;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_COORDINATOR_H_
