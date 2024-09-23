// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_observing.h"

@implementation TabGridModeHolder {
  NSHashTable<id<TabGridModeObserving>>* _observers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _observers = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

- (void)setMode:(TabGridMode)mode {
  if (_mode == mode) {
    return;
  }
  _mode = mode;

  for (id<TabGridModeObserving> observer in _observers) {
    [observer tabGridModeDidChange:self];
  }
}

- (void)addObserver:(id<TabGridModeObserving>)observer {
  [_observers addObject:observer];
}

- (void)removeObserver:(id<TabGridModeObserving>)observer {
  [_observers removeObject:observer];
}

@end
