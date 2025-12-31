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

// The web state list currently observed by this mediator.
@property(nonatomic, assign) WebStateList* webStateList;

@end

@implementation AppBarMediator {
  std::unique_ptr<WebStateListObserverBridge> _observerBridge;
  raw_ptr<WebStateList> _regularWebStateList;
  raw_ptr<WebStateList> _incognitoWebStateList;
  TabGridPage _currentPage;
}

- (instancetype)initWithRegularWebStateList:(WebStateList*)regularWebStateList
                      incognitoWebStateList:
                          (WebStateList*)incognitoWebStateList {
  self = [super init];
  if (self) {
    _regularWebStateList = regularWebStateList;
    _incognitoWebStateList = incognitoWebStateList;
    _observerBridge = std::make_unique<WebStateListObserverBridge>(self);
    // The app starts in the regular tab grid.
    self.webStateList = _regularWebStateList;
    _currentPage = TabGridPageRegularTabs;
  }
  return self;
}

- (void)setConsumer:(id<AppBarConsumer>)consumer {
  _consumer = consumer;
  [self updateConsumer];
}

- (void)setIncognitoWebStateList:(WebStateList*)incognitoWebStateList {
  // TODO(crbug.com/472279443): How to handle the destruction of the incognito
  // web state list?
  _incognitoWebStateList = incognitoWebStateList;
  if (_currentPage == TabGridPageIncognitoTabs) {
    self.webStateList = _incognitoWebStateList;
    [self updateConsumer];
  }
}

- (void)disconnect {
  self.consumer = nil;
  if (self.webStateList) {
    self.webStateList->RemoveObserver(_observerBridge.get());
    self.webStateList = nullptr;
  }
  _observerBridge.reset();
  _regularWebStateList = nullptr;
  _incognitoWebStateList = nullptr;
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  [self updateConsumer];
}

#pragma mark - TabGridObserving

- (void)willEnterTabGrid {
  [self.consumer willEnterTabGrid];
}

- (void)willExitTabGrid {
  [self.consumer willExitTabGrid];
}

- (void)willChangePageTo:(TabGridPage)page {
  _currentPage = page;
  switch (page) {
    case TabGridPageIncognitoTabs:
      self.webStateList = _incognitoWebStateList;
      break;
    case TabGridPageRegularTabs:
      self.webStateList = _regularWebStateList;
      break;
    case TabGridPageTabGroups:
      // TODO(crbug.com/472279443): Handle tab groups page.
      break;
  }
}

#pragma mark - AppBarMutator

- (void)createNewTab {
  // TODO(crbug.com/472279443): Add the logic to add a new tab. This might be a
  // bit different if the TabGrid is presented as there is a lot of custom
  // logic.
}

#pragma mark - Properties

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

#pragma mark - Private

// Updates the consumer with the current state of the web state list.
- (void)updateConsumer {
  if (!self.consumer || !self.webStateList) {
    return;
  }
  [self.consumer updateTabCount:self.webStateList->count()];
}

@end
