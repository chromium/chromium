// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section_mediator.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FeedTopSectionMediator ()

@property(nonatomic, assign) ChromeBrowserState* browserState;

@end

@implementation FeedTopSectionMediator

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState;
  }
  return self;
}

- (void)setUp {
}

- (void)dealloc {
  [self shutdown];
}

- (void)shutdown {
}

@end
