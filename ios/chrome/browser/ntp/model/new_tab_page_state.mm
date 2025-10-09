// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/new_tab_page_state.h"

@implementation NewTabPageState

- (instancetype)init {
  self = [super init];
  if (self) {
    // The default scroll position of the NTP. We don't know what the top
    // position of the NTP is in advance, so we use `-CGFLOAT_MAX` to indicate
    // that it's scrolled to top.
    _scrollPosition = -CGFLOAT_MAX;
  }
  return self;
}

- (instancetype)initWithScrollPosition:(CGFloat)scrollPosition {
  self = [super init];
  if (self) {
    _scrollPosition = scrollPosition;
  }
  return self;
}

@end
