// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_SUBCLASSING_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"

@interface BaseGridViewController (Subclassing) <UICollectionViewDelegate>

@property(nonatomic, weak, readonly) UICollectionView* collectionView;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_SUBCLASSING_H_
