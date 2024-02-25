// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Container which contains a grid. It is the link between Tab Grid and Grid
// itself. The grid can be the incognito grid, regular grid, remote grid, but it
// can be also the disabled grid when incognito, regular or remote are disabled
// by any policy.
@interface GridContainerViewController : UIViewController

// The currently embedded view controller. It is sized to fit the container.
@property(nonatomic, strong) UIViewController* containedViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONTAINER_VIEW_CONTROLLER_H_
