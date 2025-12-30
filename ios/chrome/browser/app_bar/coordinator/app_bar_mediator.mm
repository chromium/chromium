// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"

#import <memory>

#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"

@interface AppBarMediator () <WebStateListObserving>
@end

@implementation AppBarMediator {
  std::unique_ptr<WebStateListObserverBridge> _observerBridge;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _observerBridge = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_observerBridge.get());
  }
  _webStateList = webStateList;
  if (_webStateList) {
    _webStateList->AddObserver(_observerBridge.get());
  }
  [self updateConsumer];
}

- (void)setConsumer:(id<AppBarConsumer>)consumer {
  _consumer = consumer;
  [self updateConsumer];
}

- (void)disconnect {
  self.consumer = nil;
  if (_webStateList) {
    _webStateList->RemoveObserver(_observerBridge.get());
    _webStateList = nullptr;
  }
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  [self updateConsumer];
}

#pragma mark - AppBarMutator

- (void)createNewTab {
  // TODO(crbug.com/472279443): Add the logic to add a new tab. This might be a
  // bit different if the TabGrid is presented as there is a lot of custom
  // logic.
}

#pragma mark - Private

// Updates the consumer with the current state of the web state list.
- (void)updateConsumer {
  if (!self.consumer || !self.webStateList) {
    return;
  }
  [self.consumer updateTabCount:self.webStateList->count()];
}

@end
