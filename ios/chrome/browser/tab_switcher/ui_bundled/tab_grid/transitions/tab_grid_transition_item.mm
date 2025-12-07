// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/tab_grid_transition_item.h"

@interface TabGridTransitionItem ()

@property(nonatomic, weak, readwrite) UIImage* snapshot;
@property(nonatomic, assign, readwrite) CGRect originalFrame;

@end

@implementation TabGridTransitionItem

+ (instancetype)itemWithSnapshot:(UIImage*)snapshot
                   originalFrame:(CGRect)originalFrame {
  TabGridTransitionItem* item = [[self alloc] init];
  item.snapshot = snapshot;
  item.originalFrame = originalFrame;
  return item;
}

@end
