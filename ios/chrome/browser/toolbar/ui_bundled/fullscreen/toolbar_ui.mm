// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbar_ui.h"

#import "base/observer_list.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbar_ui_observer.h"

@implementation ToolbarUIState {
  base::ObserverList<ToolbarUIObserver> observers_;
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

    for (ToolbarUIObserver& observer : observers_) {
      observer.OnTopToolbarHeightChanged();
    }
  }
  if (_collapsedBottomToolbarHeight != collapsedBottomToolbarHeight ||
      _expandedBottomToolbarHeight != expandedBottomToolbarHeight) {
    _collapsedBottomToolbarHeight = collapsedBottomToolbarHeight;
    _expandedBottomToolbarHeight = expandedBottomToolbarHeight;

    for (ToolbarUIObserver& observer : observers_) {
      observer.OnBottomToolbarHeightChanged();
    }
  }
}

- (void)addObserver:(ToolbarUIObserver*)observer {
  observers_.AddObserver(observer);
}

- (void)removeObserver:(ToolbarUIObserver*)observer {
  observers_.RemoveObserver(observer);
}

@end
