// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item_fetch_info.h"

@implementation TabGroupItemFetchInfo {
  // Track the remaining fetch count.
  NSInteger _remainingFetchesCount;
}

- (instancetype)initWithRequestID:(NSUUID*)requestID
                initialFetchCount:(NSInteger)initialCount {
  self = [super init];
  if (self) {
    _requestID = requestID;
    _remainingFetchesCount = initialCount;
  }
  return self;
}

- (NSInteger)decrementRemainingFetches {
  return --_remainingFetchesCount;
}

- (NSInteger)currentRemainingFetches {
  return _remainingFetchesCount;
}

@end
