// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"

#import "base/ios/crb_protocol_observers.h"

@interface TabGridStateObserverList
    : CRBProtocolObservers <TabGridStateObserver>
@end
@implementation TabGridStateObserverList
@end

@implementation TabGridState {
  TabGridStateObserverList* _observers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _observers = [TabGridStateObserverList
        observersWithProtocol:@protocol(TabGridStateObserver)];
  }
  return self;
}

- (void)setTabGridVisible:(BOOL)tabGridVisible {
  if (tabGridVisible == _tabGridVisible) {
    return;
  }
  _tabGridVisible = tabGridVisible;
  if (tabGridVisible) {
    [_observers willEnterTabGrid];
  } else {
    [_observers willExitTabGrid];
  }
}

- (void)setCurrentPage:(TabGridPage)currentPage {
  if (_currentPage == currentPage) {
    return;
  }
  _currentPage = currentPage;
  [_observers willChangePageTo:_currentPage];
}

- (void)addObserver:(id<TabGridStateObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<TabGridStateObserver>)observer {
  [_observers removeObserver:observer];
}

@end
