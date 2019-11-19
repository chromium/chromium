// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_web_state_list_observing_delegate.h"

#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeWebStateListObservingDelegate {
  // Bridges C++ WebStateListObserver methods to this
  // WebStateListObservingDelegate.
  std::unique_ptr<WebStateListObserver> _webStateListObserver;
}

@synthesize didMoveWebStateWasCalled = _didMoveWebStateWasCalled;

- (instancetype)init {
  if ((self = [super init])) {
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)observeWebStateList:(WebStateList*)webStateList {
  webStateList->AddObserver(_webStateListObserver.get());
}

- (void)stopObservingWebStateList:(WebStateList*)webStateList {
  webStateList->RemoveObserver(_webStateListObserver.get());
}

#pragma mark - WebStateListObserving protocol
- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex {
  _didMoveWebStateWasCalled = YES;
}
@end
