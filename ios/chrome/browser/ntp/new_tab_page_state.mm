// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/new_tab_page_state.h"

@implementation NewTabPageState

- (instancetype)init {
  self = [super init];
  if (self) {
    _scrollPosition = [self defaultScrollPosition];
    _selectedFeed = [self defaultFeedType];
  }
  return self;
}

- (instancetype)initWithScrollPosition:(CGFloat)scrollPosition
                          selectedFeed:(FeedType)selectedFeed {
  self = [super init];
  if (self) {
    _scrollPosition = scrollPosition;
    _selectedFeed = selectedFeed;
  }
  return self;
}

#pragma mark - Private

// The default scroll position of the NTP. We don't know what the top position
// of the NTP is in advance, so we use `-CGFLOAT_MAX` to indicate that it's
// scrolled to top.
- (CGFloat)defaultScrollPosition {
  return -CGFLOAT_MAX;
}

// The default visible feed type for an NTP.
- (FeedType)defaultFeedType {
  return FeedTypeDiscover;
}

@end
