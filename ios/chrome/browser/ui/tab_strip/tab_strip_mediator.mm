// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_strip/tab_strip_mediator.h"

#import "ios/chrome/browser/ui/tab_strip/tab_strip_consumer.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabStripMediator ()
// The consumer for this object.
@property(nonatomic, weak) id<TabStripConsumer> consumer;
@end

@implementation TabStripMediator

- (instancetype)initWithConsumer:(id<TabStripConsumer>)consumer {
  if (self = [super init]) {
    _consumer = consumer;
  }
  return self;
}

#pragma mark - Public properties

- (void)setWebStateList:(WebStateList*)webStateList {
  _webStateList = webStateList;
  if (_webStateList) {
    DCHECK_GE(_webStateList->count(), 0);
    [_consumer setTabsCount:static_cast<NSUInteger>(_webStateList->count())];
  }
}

@end
