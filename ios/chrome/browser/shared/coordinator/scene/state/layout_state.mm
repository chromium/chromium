// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"

#import "base/ios/crb_protocol_observers.h"

@interface LayoutStateObserverList : CRBProtocolObservers <LayoutStateObserver>
@end

@implementation LayoutStateObserverList
@end

@implementation LayoutState {
  LayoutStateObserverList* _observers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _observers = [LayoutStateObserverList
        observersWithProtocol:@protocol(LayoutStateObserver)];
  }
  return self;
}

- (void)setContainedLayoutActive:(BOOL)active {
  if (_containedLayoutActive == active) {
    return;
  }

  [_observers layoutState:self
      willChangeContainedLayout:active
      withTransitionCoordinator:nil];
  _containedLayoutActive = active;
}

- (void)setContainedLayoutActive:(BOOL)active
       withTransitionCoordinator:(id<LayoutTransitionCoordinating>)coordinator {
  if (_containedLayoutActive == active) {
    return;
  }

  [_observers layoutState:self
      willChangeContainedLayout:active
      withTransitionCoordinator:coordinator];

  _containedLayoutActive = active;
}

- (void)setContainedLayoutSupported:(BOOL)containedLayoutSupported {
  if (_containedLayoutSupported == containedLayoutSupported) {
    return;
  }
  _containedLayoutSupported = containedLayoutSupported;
  [_observers layoutState:self
      didChangeContainedLayoutSupported:containedLayoutSupported];
}

- (void)setWindowedMode:(BOOL)windowedMode {
  if (_windowedMode == windowedMode) {
    return;
  }
  _windowedMode = windowedMode;
  [_observers layoutState:self didChangeWindowedMode:windowedMode];
}

- (void)addObserver:(id<LayoutStateObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<LayoutStateObserver>)observer {
  [_observers removeObserver:observer];
}

@end
