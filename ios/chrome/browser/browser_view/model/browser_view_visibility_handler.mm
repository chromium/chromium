// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/model/browser_view_visibility_handler.h"

#import "base/memory/raw_ptr.h"

@implementation BrowserViewVisibilityHandler {
  /// Callback to be invoked when browser view visibility state changes.
  BrowserViewVisibilityStateChangeCallback _visibilityChangeCallback;
}

- (instancetype)initWithVisibilityChangeCallback:
    (const BrowserViewVisibilityStateChangeCallback&)callback {
  self = [super init];
  if (self) {
    _visibilityChangeCallback = callback;
  }
  return self;
}

#pragma mark - BrowserViewVisibilityConsumer

- (void)browserViewDidTransitionToVisibilityState:
            (BrowserViewVisibilityState)currentState
                                        fromState:(BrowserViewVisibilityState)
                                                      previousState {
  _visibilityChangeCallback.Run(currentState, previousState);
}

@end
