// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/text_zoom/text_zoom_mediator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/text_zoom_commands.h"
#import "ios/chrome/browser/ui/text_zoom/text_zoom_consumer.h"
#import "ios/chrome/browser/web/font_size/font_size_tab_helper.h"
#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TextZoomMediator () <WebStateListObserving, CRWWebStateObserver> {
  std::unique_ptr<WebStateListObserver> _webStateListObserver;
  std::unique_ptr<web::WebStateObserverBridge> _observer;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;
}

// The WebStateList that this mediator listens for any changes on the active web
// state.
@property(nonatomic, readonly) WebStateList* webStateList;

// The active WebState's font size tab helper.
@property(nonatomic, readonly) FontSizeTabHelper* fontSizeTabHelper;

// Handler for any TextZoomCommands.
@property(nonatomic, weak) id<TextZoomCommands> commandHandler;

@end

@implementation TextZoomMediator

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                      commandHandler:(id<TextZoomCommands>)commandHandler {
  self = [super init];
  if (self) {
    _commandHandler = commandHandler;

    DCHECK(webStateList);
    // Set up the WebStateList and its observer.
    _webStateList = webStateList;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());

    // Set up the active web state observer.
    _observer = std::make_unique<web::WebStateObserverBridge>(self);
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        webStateList, _observer.get());
  }
  return self;
}

- (void)dealloc {
  // |-disconnect| must be called before deallocation.
  DCHECK(!_webStateList);
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver = nullptr;
    _webStateList = nullptr;

    _forwarder = nullptr;
  }
}

#pragma mark - Accessors

- (FontSizeTabHelper*)fontSizeTabHelper {
  if (!self.webStateList) {
    return nullptr;
  }
  web::WebState* activeWebState = self.webStateList->GetActiveWebState();
  return activeWebState ? FontSizeTabHelper::FromWebState(activeWebState)
                        : nullptr;
}

- (void)setConsumer:(id<TextZoomConsumer>)consumer {
  _consumer = consumer;
  [self updateConsumerState];
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  DCHECK_EQ(self.webStateList, webStateList);
  if (atIndex == webStateList->active_index()) {
    [self.commandHandler closeTextZoom];
  }
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  [self.commandHandler closeTextZoom];
}

#pragma mark - TextZoomHandler

- (void)zoomIn {
  self.fontSizeTabHelper->UserZoom(ZOOM_IN);
  [self updateConsumerState];
}

- (void)zoomOut {
  self.fontSizeTabHelper->UserZoom(ZOOM_OUT);
  [self updateConsumerState];
}

- (void)resetZoom {
  self.fontSizeTabHelper->UserZoom(ZOOM_RESET);
  [self updateConsumerState];
}

- (void)updateConsumerState {
  [self.consumer setZoomInEnabled:self.fontSizeTabHelper->CanUserZoomIn()];
  [self.consumer setZoomOutEnabled:self.fontSizeTabHelper->CanUserZoomOut()];
  [self.consumer
      setResetZoomEnabled:self.fontSizeTabHelper->CanUserResetZoom()];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self.commandHandler closeTextZoom];
}

@end
