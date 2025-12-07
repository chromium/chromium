// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/tab_grid_transition_layout.h"

#import "base/check.h"

@interface TabGridTransitionLayout ()

@property(nonatomic, strong, readwrite) TabGridTransitionItem* activeCell;

@end

@implementation TabGridTransitionLayout

+ (instancetype)layoutWithActiveCell:(TabGridTransitionItem*)activeCell
                          activeGrid:(UIViewController*)activeGrid {
  TabGridTransitionLayout* layout = [[self alloc] init];
  layout.activeCell = activeCell;
  layout.activeGrid = activeGrid;
  return layout;
}

@end
