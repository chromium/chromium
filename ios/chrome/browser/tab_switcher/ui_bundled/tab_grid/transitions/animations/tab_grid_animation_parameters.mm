// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_animation_parameters.h"

@implementation TabGridAnimationParameters

- (instancetype)initWithDestinationFrame:(CGRect)destinationFrame
                             originFrame:(CGRect)originFrame
                              activeGrid:(UIViewController*)activeGrid
                              pinnedTabs:(UIViewController*)pinnedTabs
                        activeCellPinned:(BOOL)activeCellPinned
                            animatedView:(UIView*)animatedView
                         contentSnapshot:(UIImage*)contentSnapshot
                        topToolbarHeight:(CGFloat)topToolbarHeight
                     bottomToolbarHeight:(CGFloat)bottomToolbarHeight
                  topToolbarSnapshotView:(UIView*)topToolbarSnapshotView
               bottomToolbarSnapshotView:(UIView*)bottomToolbarSnapshotView
                   shouldScaleTopToolbar:(BOOL)shouldScaleTopToolbar
                               incognito:(BOOL)incognito
                        topToolbarHidden:(BOOL)topToolbarHidden {
  self = [super init];
  if (self) {
    _destinationFrame = destinationFrame;
    _originFrame = originFrame;
    _animatedView = animatedView;
    _contentSnapshot = contentSnapshot;
    _topToolbarHeight = topToolbarHeight;
    _bottomToolbarHeight = bottomToolbarHeight;
    _topToolbarSnapshotView = topToolbarSnapshotView;
    _bottomToolbarSnapshotView = bottomToolbarSnapshotView;
    _activeGrid = activeGrid;
    _pinnedTabs = pinnedTabs;
    _activeCellPinned = activeCellPinned;
    _shouldScaleTopToolbar = shouldScaleTopToolbar;
    _incognito = incognito;
    _topToolbarHidden = topToolbarHidden;
  }
  return self;
}

@end
