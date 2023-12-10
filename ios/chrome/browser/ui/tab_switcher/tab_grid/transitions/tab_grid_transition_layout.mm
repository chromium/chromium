// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_layout.h"

#import "base/check.h"

@interface TabGridTransitionLayout ()

@property(nonatomic, strong, readwrite) TabGridTransitionItem* activeCell;

@end

@implementation TabGridTransitionLayout

+ (instancetype)layoutWithActiveCell:(TabGridTransitionItem*)activeCell {
  TabGridTransitionLayout* layout = [[self alloc] init];
  layout.activeCell = activeCell;
  return layout;
}

@end
