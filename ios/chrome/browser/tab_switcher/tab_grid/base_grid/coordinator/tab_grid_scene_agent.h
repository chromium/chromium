// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_GRID_BASE_GRID_COORDINATOR_TAB_GRID_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_GRID_BASE_GRID_COORDINATOR_TAB_GRID_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"

@protocol TabGridObserving;

// Scene agent that can be observed to know when the tab grid is entered/exited.
@interface TabGridSceneAgent : ObservingSceneAgent

// Alerts the agent that the tab grid will be entered.
- (void)willEnterTabGrid;

// Alerts the agent that the tab grid will be exited.
- (void)willExitTabGrid;

// Alerts the agent that the tab grid will change page.
- (void)willChangePageTo:(TabGridPage)page;

// Adds an observer to receive tab grid events.
- (void)addObserver:(id<TabGridObserving>)observer;

// Removes a previously an observer.
- (void)removeObserver:(id<TabGridObserving>)observer;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_GRID_BASE_GRID_COORDINATOR_TAB_GRID_SCENE_AGENT_H_
