// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_item.h"

@interface TabGridTransitionItem ()

@property(nonatomic, strong, readwrite) UIView* view;
@property(nonatomic, assign, readwrite) CGRect originalFrame;

@end

@implementation TabGridTransitionItem

+ (instancetype)itemWithView:(UIView*)view originalFrame:(CGRect)originalFrame {
  TabGridTransitionItem* item = [[self alloc] init];
  item.view = view;
  item.originalFrame = originalFrame;
  return item;
}

@end
