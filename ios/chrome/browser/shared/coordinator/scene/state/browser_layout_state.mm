// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/state/browser_layout_state.h"

#import "base/ios/crb_protocol_observers.h"

@interface BrowserLayoutStateObserverList
    : CRBProtocolObservers <BrowserLayoutStateObserver>
@end

@implementation BrowserLayoutStateObserverList
@end

@implementation BrowserLayoutState {
  BrowserLayoutStateObserverList* _observers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _observers = [BrowserLayoutStateObserverList
        observersWithProtocol:@protocol(BrowserLayoutStateObserver)];
  }
  return self;
}

- (void)setToolbarPosition:(ToolbarPosition)toolbarPosition
                   passKey:(LayoutStateToolbarPassKey)passKey {
  if (_toolbarPosition == toolbarPosition) {
    return;
  }
  _toolbarPosition = toolbarPosition;
  [_observers browserLayoutState:self didChangeToolbarPosition:toolbarPosition];
}

- (void)addObserver:(id<BrowserLayoutStateObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<BrowserLayoutStateObserver>)observer {
  [_observers removeObserver:observer];
}

@end
