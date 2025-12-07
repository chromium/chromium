// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/coordinator/tab_grid_scene_agent.h"

#import "base/ios/crb_protocol_observers.h"
#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/coordinator/tab_grid_observing.h"

@interface TabGridSceneAgentObserverList
    : CRBProtocolObservers <TabGridObserving>
@end
@implementation TabGridSceneAgentObserverList
@end

@implementation TabGridSceneAgent {
  TabGridSceneAgentObserverList* _observers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _observers = [TabGridSceneAgentObserverList
        observersWithProtocol:@protocol(TabGridObserving)];
  }
  return self;
}

- (void)willEnterTabGrid {
  [_observers willEnterTabGrid];
}

- (void)willExitTabGrid {
  [_observers willExitTabGrid];
}

- (void)addObserver:(id<TabGridObserving>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<TabGridObserving>)observer {
  [_observers removeObserver:observer];
}

@end
