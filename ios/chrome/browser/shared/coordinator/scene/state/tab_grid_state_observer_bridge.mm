// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state_observer_bridge.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state_observer.h"

@implementation TabGridStateObserverBridge {
  raw_ptr<TabGridStateObserver> _observer;
}

- (instancetype)initWithObserver:(TabGridStateObserver*)observer {
  self = [super init];
  if (self) {
    CHECK(observer);
    _observer = observer;
  }
  return self;
}

- (void)willEnterTabGrid {
  _observer->WillEnterTabGrid();
}

- (void)willExitTabGrid {
  _observer->WillExitTabGrid();
}

- (void)willChangePageTo:(TabGridPage)page {
  _observer->WillChangePageTo(page);
}

- (void)willShowTabGroup:(const TabGroup*)group {
  _observer->WillShowTabGroup(group);
}

- (void)willHideTabGroup {
  _observer->WillHideTabGroup();
}

- (void)tabGridStateModeDidChange:(TabGridState*)tabGridState {
  _observer->TabGridStateModeDidChange(tabGridState);
}

@end
