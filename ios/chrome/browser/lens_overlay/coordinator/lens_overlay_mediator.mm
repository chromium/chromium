// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"

#import <memory>

#import "ios/chrome/browser/lens_overlay/ui/lens_toolbar_consumer.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "url/gurl.h"

@interface LensOverlayMediator () <CRWWebStateObserver>

@end

@implementation LensOverlayMediator {
  /// Bridges C++ WebStateObserver methods to this mediator.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
  }
  return self;
}

- (void)startWithSnapshot:(UIImage*)snapshot {
  [self.snapshotConsumer loadSnapshot:snapshot];
}

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
  _webState = webState;
  if (_webState) {
    _webState->AddObserver(_webStateObserverBridge.get());
  }
}

- (void)disconnect {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webState = nullptr;
  }
  _webStateObserverBridge.reset();
}

#pragma mark - LensOverlaySelectionDelegate

- (void)selectionUI:(id)selectionUI
           performedSelection:(id<LensSelection>)selection
    constructedResultsPageURL:(GURL)resultsPageURL
               suggestSignals:(NSString*)iil {
  [self.resultConsumer loadResultsURL:resultsPageURL];
}

- (void)selectionUI:(id)selectionUI
    encounteredError:(NSError*)error
       withSelection:(id<LensSelection>)selection {
}

- (void)selectionUISuccessfullyCompletedFullImageRequest:(id)selectionUI {
}

#pragma mark - Omnibox

#pragma mark CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  [self.omniboxCoordinator updateOmniboxState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webState = nullptr;
  }
}

#pragma mark LensOmniboxClientDelegate

- (void)omniboxDidAcceptText:(const std::u16string&)text
              destinationURL:(const GURL&)destinationURL {
  [self defocusOmnibox];
  [self.resultConsumer loadResultsURL:destinationURL];
}

#pragma mark LensOmniboxMutator

- (void)focusOmnibox {
  [self.omniboxCoordinator focusOmnibox];
  [self.toolbarConsumer setOmniboxFocused:YES];
}

- (void)defocusOmnibox {
  [self.omniboxCoordinator endEditing];
  [self.toolbarConsumer setOmniboxFocused:NO];
}

#pragma mark OmniboxFocusDelegate

- (void)omniboxDidBecomeFirstResponder {
  [self focusOmnibox];
}

- (void)omniboxDidResignFirstResponder {
  [self defocusOmnibox];
}

@end
