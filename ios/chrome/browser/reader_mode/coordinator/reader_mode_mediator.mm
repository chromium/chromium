// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_mediator.h"

#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_consumer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface ReaderModeMediator () <WebStateListObserving, CRWWebStateObserver>
@end

@implementation ReaderModeMediator {
  raw_ptr<WebStateList> _webStateList;
  raw_ptr<BwgService> _BWGService;
  raw_ptr<dom_distiller::DistilledPagePrefs> _distilledPagePrefs;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  // WebState whose view is currently being shown in the consumer. It is
  // necessary to keep track of it so `WasHidden()` can be called when another
  // WebState is about to be shown instead.
  raw_ptr<web::WebState> _contentWebState;
  std::unique_ptr<web::WebStateObserverBridge> _contentWebStateObserverBridge;
}

#pragma mark - Initialization

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                          BWGService:(BwgService*)BWGService
                  distilledPagePrefs:
                      (dom_distiller::DistilledPagePrefs*)distilledPagePrefs {
  self = [super init];
  if (self) {
    CHECK(webStateList);
    _webStateList = webStateList;
    _BWGService = BWGService;
    _distilledPagePrefs = distilledPagePrefs;
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserverBridge.get());
  }
  return self;
}

#pragma mark - Properties

- (dom_distiller::DistilledPagePrefs*)distilledPagePrefs {
  return _distilledPagePrefs;
}

#pragma mark - ReaderModeMutator

- (void)setDefaultTheme:(dom_distiller::mojom::Theme)theme {
  if (_distilledPagePrefs) {
    _distilledPagePrefs->SetDefaultTheme(theme);
  }
}

- (void)setConsumer:(id<ReaderModeConsumer>)consumer {
  CHECK(consumer);
  _consumer = consumer;
  [self updateContentWithNewActiveWebState:_webStateList->GetActiveWebState()];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change()) {
    [self updateContentWithNewActiveWebState:status.new_active_web_state];
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  _webStateList->RemoveObserver(_webStateListObserverBridge.get());
  _webStateListObserverBridge.reset();
  _webStateList = nullptr;
}

#pragma mark - Public

- (BOOL)BWGAvailableForProfile {
  return _BWGService && _BWGService->IsProfileEligibleForBwg();
}

- (void)disconnect {
  if (_contentWebState) {
    _contentWebState->RemoveObserver(_contentWebStateObserverBridge.get());
    _contentWebStateObserverBridge.reset();
    _contentWebState = nullptr;
  }
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserverBridge.get());
    _webStateListObserverBridge.reset();
    _webStateList = nullptr;
  }
  _BWGService = nullptr;
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_contentWebState, webState);
  [self updateContentWithNewActiveWebState:nullptr];
}

#pragma mark - Private

// If `activeWebState` is not null, feed the Reader mode content view of
// `activeWebState` to `consumer`. Otherwise, give `nil` content view to
// consumer.
- (void)updateContentWithNewActiveWebState:(web::WebState*)activeWebState {
  if (_contentWebState) {
    _contentWebState->RemoveObserver(_contentWebStateObserverBridge.get());
    _contentWebStateObserverBridge.reset();
    [self.consumer removeContentView];
    _contentWebState->WasHidden();
    _contentWebState = nullptr;
  }

  // If there is a new content view, feed it to consumer.
  if (!activeWebState) {
    return;
  }
  ReaderModeTabHelper* readerModeTabHelper =
      ReaderModeTabHelper::FromWebState(activeWebState);
  web::WebState* readerModeWebState =
      readerModeTabHelper->GetReaderModeWebState();
  if (!readerModeWebState) {
    return;
  }

  _contentWebState = readerModeWebState;
  const OverscrollStyle overscrollStyle =
      _contentWebState->GetBrowserState()->IsOffTheRecord()
          ? OverscrollStyle::REGULAR_PAGE_INCOGNITO
          : OverscrollStyle::REGULAR_PAGE_NON_INCOGNITO;
  [self.consumer setContentView:_contentWebState->GetView()
                   webViewProxy:_contentWebState->GetWebViewProxy()
                overscrollStyle:overscrollStyle];
  _contentWebState->WasShown();
  _contentWebStateObserverBridge =
      std::make_unique<web::WebStateObserverBridge>(self);
  _contentWebState->AddObserver(_contentWebStateObserverBridge.get());
}

@end
