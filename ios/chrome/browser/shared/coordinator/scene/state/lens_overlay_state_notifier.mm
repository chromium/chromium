// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/state/lens_overlay_state_notifier.h"

#import "base/ios/crb_protocol_observers.h"

@interface LensOverlayStateNotifierObserverList
    : CRBProtocolObservers <LensOverlayStateNotifierObserver>
@end

@implementation LensOverlayStateNotifierObserverList
@end

@implementation LensOverlayStateNotifier {
  LensOverlayStateNotifierObserverList* _observers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _observers = [LensOverlayStateNotifierObserverList
        observersWithProtocol:@protocol(LensOverlayStateNotifierObserver)];
  }
  return self;
}

- (void)lensOverlayDidPrepare {
  [_observers lensOverlayDidPrepare:self];
}

- (void)lensOverlayWillAppear {
  [_observers lensOverlayWillAppear:self];
}

- (void)lensOverlayWillDisappear {
  [_observers lensOverlayWillDisappear:self];
}

- (void)lensOverlayDidDisappear {
  [_observers lensOverlayDidDisappear:self];
}

- (void)lensOverlayDidReadjustPresentation {
  [_observers lensOverlayDidReadjustPresentation:self];
}

- (void)addObserver:(id<LensOverlayStateNotifierObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<LensOverlayStateNotifierObserver>)observer {
  [_observers removeObserver:observer];
}

@end
