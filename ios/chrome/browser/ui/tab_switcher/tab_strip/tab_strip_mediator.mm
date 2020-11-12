// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_mediator.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_consumer.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabStripMediator () <WebStateListObserving> {
  // Bridge C++ WebStateListObserver methods to this TabStripController.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}

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

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver = nullptr;
    _webStateList = nullptr;
  }
}

#pragma mark - Public properties

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;

  if (_webStateList) {
    DCHECK_GE(_webStateList->count(), 0);
    [_consumer setTabsCount:static_cast<NSUInteger>(_webStateList->count())];
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
  }
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  [_consumer setTabsCount:static_cast<NSUInteger>(_webStateList->count())];
}

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [_consumer setTabsCount:static_cast<NSUInteger>(_webStateList->count())];
}

@end
