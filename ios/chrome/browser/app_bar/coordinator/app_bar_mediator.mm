// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"

#import <memory>

#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/web/public/web_state.h"

@interface AppBarMediator () <IncognitoStateObserver,
                              TabGridStateObserver,
                              WebStateListObserving>

// The web state list currently observed by this mediator.
@property(nonatomic, assign) WebStateList* currentWebStateList;

@end

@implementation AppBarMediator {
  std::unique_ptr<WebStateListObserverBridge> _observerBridge;
  raw_ptr<WebStateList> _regularWebStateList;
  raw_ptr<WebStateList> _incognitoWebStateList;
  TabGridPage _currentPage;
  TabGridState* _tabGridState;
  IncognitoState* _incognitoState;
}

- (instancetype)initWithRegularWebStateList:(WebStateList*)regularWebStateList
                      incognitoWebStateList:(WebStateList*)incognitoWebStateList
                               tabGridState:(TabGridState*)tabGridState
                             incognitoState:(IncognitoState*)incognitoState {
  self = [super init];
  if (self) {
    _regularWebStateList = regularWebStateList;
    _incognitoWebStateList = incognitoWebStateList;
    _observerBridge = std::make_unique<WebStateListObserverBridge>(self);

    _tabGridState = tabGridState;
    [_tabGridState addObserver:self];

    _incognitoState = incognitoState;
    [_incognitoState addObserver:self];

    if (_tabGridState.tabGridVisible) {
      [self updateForTabGridPage:_tabGridState.currentPage];
    } else {
      [self updateForIncognitoVisible:_incognitoState.incognitoContentVisible];
    }
  }
  return self;
}

- (void)setConsumer:(id<AppBarConsumer>)consumer {
  _consumer = consumer;
  [self updateConsumer];
}

- (void)setIncognitoWebStateList:(WebStateList*)incognitoWebStateList {
  _incognitoWebStateList = incognitoWebStateList;
  if (_tabGridState.tabGridVisible &&
      _currentPage == TabGridPageIncognitoTabs) {
    self.currentWebStateList = _incognitoWebStateList;
    [self updateConsumer];
  } else if (_incognitoState.incognitoContentVisible) {
    self.currentWebStateList = _incognitoWebStateList;
  }
}

- (void)disconnect {
  self.consumer = nil;
  if (self.currentWebStateList) {
    self.currentWebStateList->RemoveObserver(_observerBridge.get());
    self.currentWebStateList = nullptr;
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

#pragma mark - IncognitoStateObserver

- (void)willEnterIncognitoForState:(IncognitoState*)incognitoState {
  if (_tabGridState.tabGridVisible) {
    return;
  }
  self.currentWebStateList = _incognitoWebStateList;
}

- (void)willExitIncognitoForState:(IncognitoState*)incognitoState {
  if (_tabGridState.tabGridVisible) {
    return;
  }
  self.currentWebStateList = _regularWebStateList;
}

#pragma mark - TabGridStateObserver

- (void)willEnterTabGrid {
  _currentPage = _tabGridState.currentPage;
  [self.consumer willEnterTabGrid];
}

- (void)willExitTabGrid {
  [self.consumer willExitTabGrid];
  [self updateForIncognitoVisible:_incognitoState.incognitoContentVisible];
}

- (void)willChangePageTo:(TabGridPage)page {
  _currentPage = page;
  if (!_tabGridState.tabGridVisible) {
    return;
  }
  [self updateForTabGridPage:page];
}

#pragma mark - AppBarMutator

- (void)createNewTabFromView:(UIView*)sender {
  if (_tabGridState.tabGridVisible) {
    // TODO(crbug.com/472279443): Add the logic to add a new tab from the
    // TabGrid.
  } else {
    CGPoint center = [sender.superview convertPoint:sender.center toView:nil];
    OpenNewTabCommand* command = [OpenNewTabCommand
        commandWithIncognito:_incognitoState.incognitoContentVisible
                 originPoint:center];
    [self.sceneHandler openURLInNewTab:command];

    [IntentDonationHelper donateIntent:IntentType::kOpenNewTab];
  }
}

#pragma mark - Properties

- (void)setCurrentWebStateList:(WebStateList*)currentWebStateList {
  if (_currentWebStateList) {
    _currentWebStateList->RemoveObserver(_observerBridge.get());
  }
  _currentWebStateList = currentWebStateList;
  if (_currentWebStateList) {
    _currentWebStateList->AddObserver(_observerBridge.get());
  }
  [self updateConsumer];
}

#pragma mark - Private

// Updates the consumer with the current state of the web state list.
- (void)updateConsumer {
  if (!self.consumer || !self.currentWebStateList) {
    return;
  }
  [self.consumer updateTabCount:self.currentWebStateList->count()];
}

// Updates for entering tab grid `page`.
- (void)updateForTabGridPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      self.currentWebStateList = _incognitoWebStateList;
      break;
    case TabGridPageRegularTabs:
      self.currentWebStateList = _regularWebStateList;
      break;
    case TabGridPageTabGroups:
      // TODO(crbug.com/472279443): Handle tab groups page.
      break;
  }
}

// Updates for `incognito` being visible.
- (void)updateForIncognitoVisible:(BOOL)incognitoVisible {
  if (incognitoVisible) {
    self.currentWebStateList = _incognitoWebStateList;
  } else {
    self.currentWebStateList = _regularWebStateList;
  }
}

@end
