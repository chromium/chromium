// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_font_size_utils.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_consumer.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface TextZoomMediator () <WebStateListObserving, CRWWebStateObserver>
@end

@implementation TextZoomMediator {
  // The WebStateList observed by this mediator and the observer bridge.
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<WebStateListObserver> _webStateListObserver;

  // The active WebState of the WebStateList and the observer bridge. It
  // is observed to detect navigation and to close the UI when they happen.
  raw_ptr<web::WebState> _activeWebState;
  std::unique_ptr<web::WebStateObserver> _activeWebStateObserver;

  // The handler for any TextZoom commands.
  id<TextZoomCommands> _commandHandler;

  // Distilled page prefs for reader mode.
  raw_ptr<dom_distiller::DistilledPagePrefs> _distilledPagePrefs;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                      commandHandler:(id<TextZoomCommands>)commandHandler
                  distilledPagePrefs:
                      (dom_distiller::DistilledPagePrefs*)distilledPagePrefs {
  DCHECK(webStateList);
  DCHECK(commandHandler);
  if (([super init])) {
    _webStateList = webStateList;
    _commandHandler = commandHandler;
    _activeWebState = _webStateList->GetActiveWebState();
    _distilledPagePrefs = distilledPagePrefs;

    // Create and register the observers.
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());

    _activeWebStateObserver =
        std::make_unique<web::WebStateObserverBridge>(self);
    if (_activeWebState) {
      _activeWebState->AddObserver(_activeWebStateObserver.get());
    }
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_activeWebState);
  DCHECK(!_webStateList);
}

- (void)disconnect {
  if (_activeWebState) {
    _activeWebState->RemoveObserver(_activeWebStateObserver.get());
    _activeWebStateObserver.reset();
    _activeWebState = nullptr;
  }

  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver = nullptr;
    _webStateList = nullptr;
  }
  _distilledPagePrefs = nullptr;
}

#pragma mark - Accessors

- (void)setConsumer:(id<TextZoomConsumer>)consumer {
  _consumer = consumer;
  [self updateConsumerState];
}

#pragma mark - WebStateListObserver

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (status.active_web_state_change()) {
    [self setActiveWebState:status.new_active_web_state];
    [_commandHandler closeTextZoom];
  }
}

#pragma mark - TextZoomHandler

- (void)zoomIn {
  if (!_activeWebState) {
    return;
  }
  ReaderModeTabHelper* readerModeTabHelper =
      ReaderModeTabHelper::FromWebState(_activeWebState);
  if (readerModeTabHelper && readerModeTabHelper->IsActive()) {
    IncreaseReaderModeFontSize(_distilledPagePrefs);
  } else {
    FontSizeTabHelper* fontSizeTabHelper =
        FontSizeTabHelper::FromWebState(_activeWebState);
    if (fontSizeTabHelper) {
      fontSizeTabHelper->UserZoom(ZOOM_IN);
    }
  }
  [self updateConsumerState];
}

- (void)zoomOut {
  if (!_activeWebState) {
    return;
  }
  ReaderModeTabHelper* readerModeTabHelper =
      ReaderModeTabHelper::FromWebState(_activeWebState);
  if (readerModeTabHelper && readerModeTabHelper->IsActive()) {
    DecreaseReaderModeFontSize(_distilledPagePrefs);
  } else {
    FontSizeTabHelper* fontSizeTabHelper =
        FontSizeTabHelper::FromWebState(_activeWebState);
    if (fontSizeTabHelper) {
      fontSizeTabHelper->UserZoom(ZOOM_OUT);
    }
  }
  [self updateConsumerState];
}

- (void)resetZoom {
  if (!_activeWebState) {
    return;
  }
  ReaderModeTabHelper* readerModeTabHelper =
      ReaderModeTabHelper::FromWebState(_activeWebState);
  if (readerModeTabHelper && readerModeTabHelper->IsActive()) {
    ResetReaderModeFontSize(_distilledPagePrefs);
  } else {
    FontSizeTabHelper* fontSizeTabHelper =
        FontSizeTabHelper::FromWebState(_activeWebState);
    if (fontSizeTabHelper) {
      fontSizeTabHelper->UserZoom(ZOOM_RESET);
    }
  }
  [self updateConsumerState];
}

- (void)updateConsumerState {
  if (!_activeWebState) {
    return;
  }
  ReaderModeTabHelper* readerModeTabHelper =
      ReaderModeTabHelper::FromWebState(_activeWebState);
  if (readerModeTabHelper && readerModeTabHelper->IsActive()) {
    [_consumer
        setZoomInEnabled:CanIncreaseReaderModeFontSize(_distilledPagePrefs)];
    [_consumer
        setZoomOutEnabled:CanDecreaseReaderModeFontSize(_distilledPagePrefs)];
    [_consumer
        setResetZoomEnabled:CanResetReaderModeFontSize(_distilledPagePrefs)];
  } else {
    FontSizeTabHelper* fontSizeTabHelper =
        FontSizeTabHelper::FromWebState(_activeWebState);
    if (fontSizeTabHelper) {
      [_consumer setZoomInEnabled:fontSizeTabHelper->CanUserZoomIn()];
      [_consumer setZoomOutEnabled:fontSizeTabHelper->CanUserZoomOut()];
      [_consumer setResetZoomEnabled:fontSizeTabHelper->CanUserResetZoom()];
    }
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  [self setActiveWebState:nullptr];
  [_commandHandler closeTextZoom];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [_commandHandler closeTextZoom];
}

#pragma mark - Private methods

- (void)setActiveWebState:(web::WebState*)webState {
  if (_activeWebState) {
    _activeWebState->RemoveObserver(_activeWebStateObserver.get());
    _activeWebState = nullptr;
  }

  if (webState) {
    _activeWebState = webState;
    _activeWebState->AddObserver(_activeWebStateObserver.get());
  }
}
@end
