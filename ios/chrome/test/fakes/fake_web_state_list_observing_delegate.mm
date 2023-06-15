// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_web_state_list_observing_delegate.h"

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeWebStateListObservingDelegate {
  // Bridges C++ WebStateListObserver methods to this
  // WebStateListObservingDelegate.
  std::unique_ptr<WebStateListObserver> _webStateListObserver;
}

@synthesize didWebStateWasMoved = _didWebStateWasMoved;

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

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                    selection:(const WebStateSelection&)selection {
  switch (change.type()) {
    case WebStateListChange::Type::kSelectionOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach:
      // Do nothing when a WebState is detached.
      break;
    case WebStateListChange::Type::kMove:
      _didWebStateWasMoved = YES;
      break;
    case WebStateListChange::Type::kReplace:
      // Do nothing when a new WebState is replaced.
      break;
    case WebStateListChange::Type::kInsert:
      // Do nothing when a new WebState is inserted.
      break;
  }
}

@end
