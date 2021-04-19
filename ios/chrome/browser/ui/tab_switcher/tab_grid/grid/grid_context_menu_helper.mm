// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_context_menu_helper.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation GridContextMenuHelper

#pragma mark - GridContextMenuProvider

- (UIContextMenuConfiguration*)contextMenuConfigurationForCell:(GridCell*)cell
                                                      fromView:(UIView*)view
    API_AVAILABLE(ios(13.0)) {
  return nil;
}

@end
