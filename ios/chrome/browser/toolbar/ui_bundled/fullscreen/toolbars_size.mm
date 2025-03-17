// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size.h"

#import "base/observer_list.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size_observer.h"

@implementation ToolbarsSize {
  base::ObserverList<ToolbarsSizeObserver> observers_;
}

- (instancetype)
    initWithCollapsedTopToolbarHeight:(CGFloat)collapsedTopToolbarHeight
             expandedTopToolbarHeight:(CGFloat)expandedTopToolbarHeight
          expandedBottomToolbarHeight:(CGFloat)expandedBottomToolbarHeight
         collapsedBottomToolbarHeight:(CGFloat)collapsedBottomToolbarHeight {
  self = [super init];
  if (self) {
    _collapsedTopToolbarHeight = collapsedTopToolbarHeight;
    _expandedTopToolbarHeight = expandedTopToolbarHeight;
    _expandedBottomToolbarHeight = expandedBottomToolbarHeight;
    _collapsedBottomToolbarHeight = collapsedBottomToolbarHeight;
  }
  return self;
}

- (void)setCollapsedTopToolbarHeight:(CGFloat)collapsedTopToolbarHeight
            expandedTopToolbarHeight:(CGFloat)expandedTopToolbarHeight
         expandedBottomToolbarHeight:(CGFloat)expandedBottomToolbarHeight
        collapsedBottomToolbarHeight:(CGFloat)collapsedBottomToolbarHeight {
  if (_collapsedTopToolbarHeight != collapsedTopToolbarHeight ||
      _expandedTopToolbarHeight != expandedTopToolbarHeight) {
    _collapsedTopToolbarHeight = collapsedTopToolbarHeight;
    _expandedTopToolbarHeight = expandedTopToolbarHeight;

    for (ToolbarsSizeObserver& observer : observers_) {
      observer.OnTopToolbarHeightChanged();
    }
  }
  if (_collapsedBottomToolbarHeight != collapsedBottomToolbarHeight ||
      _expandedBottomToolbarHeight != expandedBottomToolbarHeight) {
    _collapsedBottomToolbarHeight = collapsedBottomToolbarHeight;
    _expandedBottomToolbarHeight = expandedBottomToolbarHeight;

    for (ToolbarsSizeObserver& observer : observers_) {
      observer.OnBottomToolbarHeightChanged();
    }
  }
}

- (void)addObserver:(ToolbarsSizeObserver*)observer {
  observers_.AddObserver(observer);
}

- (void)removeObserver:(ToolbarsSizeObserver*)observer {
  observers_.RemoveObserver(observer);
}

@end
