// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"

@implementation ComposeboxModeHolder {
  NSHashTable<id<ComposeboxModeObserver>>* _observers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _mode = ComposeboxMode::kRegularSearch;
    _observers = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

- (void)setMode:(ComposeboxMode)mode {
  if (_mode == mode) {
    return;
  }
  _mode = mode;
  for (id<ComposeboxModeObserver> observer : _observers) {
    [observer composeboxModeDidChange:_mode];
  }
}

- (void)addObserver:(id<ComposeboxModeObserver>)observer {
  [_observers addObject:observer];
}

- (void)removeObserver:(id<ComposeboxModeObserver>)observer {
  [_observers removeObject:observer];
}

- (BOOL)isRegularSearch {
  return _mode == ComposeboxMode::kRegularSearch;
}

@end
