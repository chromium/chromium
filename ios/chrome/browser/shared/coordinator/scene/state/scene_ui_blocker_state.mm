// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/state/scene_ui_blocker_state.h"

#import "base/ios/crb_protocol_observers.h"

@interface SceneUIBlockerStateObserverList
    : CRBProtocolObservers <SceneUIBlockerStateObserver>
@end
@implementation SceneUIBlockerStateObserverList
@end

@implementation SceneUIBlockerState {
  SceneUIBlockerStateObserverList* _observers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _observers = [SceneUIBlockerStateObserverList
        observersWithProtocol:@protocol(SceneUIBlockerStateObserver)];
  }
  return self;
}

- (void)setPresentingModalOverlay:(BOOL)presentingModalOverlay {
  if (_presentingModalOverlay == presentingModalOverlay) {
    return;
  }

  if (presentingModalOverlay) {
    [_observers willShowModalOverlay];
  } else {
    [_observers willHideModalOverlay];
  }

  _presentingModalOverlay = presentingModalOverlay;

  if (!presentingModalOverlay) {
    [_observers didHideModalOverlay];
  }
}

- (void)addObserver:(id<SceneUIBlockerStateObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<SceneUIBlockerStateObserver>)observer {
  [_observers removeObserver:observer];
}

@end
