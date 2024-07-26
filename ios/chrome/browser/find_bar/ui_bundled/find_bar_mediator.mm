// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_mediator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_consumer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface FindBarMediator () <CRWWebStateObserver> {
  std::unique_ptr<web::WebStateObserverBridge> _observer;
}

// Handler for any FindInPageCommands.
@property(nonatomic, weak) id<FindInPageCommands> commandHandler;

// The WebState associated with this mediator
@property(nonatomic, assign) web::WebState* webState;

@end

@implementation FindBarMediator

- (instancetype)initWithWebState:(web::WebState*)webState
                  commandHandler:(id<FindInPageCommands>)commandHandler {
  self = [super init];
  if (self) {
    DCHECK(webState);

    _webState = webState;
    _observer = std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_observer.get());
    _commandHandler = commandHandler;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_webState);
}

- (void)disconnect {
  if (_webState && _observer) {
    _webState->RemoveObserver(_observer.get());
    _observer.reset();
    _webState = nullptr;
  }
}

#pragma mark - FindInPageResponseDelegate

- (void)findDidFinishWithUpdatedModel:(FindInPageModel*)model {
  [self.consumer updateResultsCount:model];
}

- (void)findDidStop {
  [self.commandHandler closeFindInPage];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  [self disconnect];
  [self.commandHandler hideFindUI];
}

- (void)webStateWasHidden:(web::WebState*)webState {
  [self.commandHandler hideFindUI];
}

@end
