// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_COORDINATOR_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_COORDINATOR_PRIVATE_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator.h"

@class BVCContainerViewController;

@interface TabGridCoordinator (Private)

@property(nonatomic, strong, readonly) BVCContainerViewController* bvcContainer;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_COORDINATOR_PRIVATE_H_
