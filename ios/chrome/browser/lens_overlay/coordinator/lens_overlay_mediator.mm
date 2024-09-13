// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"

#import <memory>
#import <stack>

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_toolbar_consumer.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/edit_view_animatee.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_result.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

/// History Element in the `historyStack` used for navigating to previous
/// selection/URLs.
@interface HistoryElement : NSObject
/// URL of the navigation.
@property(nonatomic, assign) GURL URL;
/// Lens result object of the navigation.
@property(nonatomic, strong) id<ChromeLensOverlayResult> lensResult;
@end

@implementation HistoryElement
@end

@interface LensOverlayMediator () <CRWWebStateObserver, SearchEngineObserving>

@end

@implementation LensOverlayMediator {
  /// Bridges C++ WebStateObserver methods to this mediator.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  /// Search engine observer.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;

  /// History stack for back navigation.
  NSMutableArray<HistoryElement*>* _historyStack;
  /// Current lens result.
  id<ChromeLensOverlayResult> _currentLensResult;
  /// Whether the URL navigation from the next lensResult should be ignored.
  /// More detail in `goBack`.
  BOOL _skipLoadingNextLensResultURL;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _historyStack = [[NSMutableArray alloc] init];
  }
  return self;
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

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  _templateURLService = templateURLService;
  if (_templateURLService) {
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    [self searchEngineChanged];
  } else {
    _searchEngineObserver.reset();
  }
}

- (void)disconnect {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webState = nullptr;
  }
  _searchEngineObserver.reset();
  _webStateObserverBridge.reset();
  [_historyStack removeAllObjects];
  _currentLensResult = nil;
  _skipLoadingNextLensResultURL = NO;
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  BOOL isLensAvailable =
      search_engines::SupportsSearchImageWithLens(_templateURLService);
  if (!isLensAvailable) {
    [self.commandsHandler destroyLensUI:YES];
  }
}

#pragma mark - Omnibox

#pragma mark CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  if (navigationContext && !navigationContext->IsSameDocument()) {
    [self addURLToHistory:navigationContext->GetUrl()];
  }
}

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
              destinationURL:(const GURL&)destinationURL
            thumbnailRemoved:(BOOL)thumbnailRemoved {
  [self defocusOmnibox];
  // Setting the query text generates new results.
  [self.lensHandler setQueryText:base::SysUTF16ToNSString(text)
                  clearSelection:thumbnailRemoved];
}

#pragma mark LensToolbarMutator

- (void)focusOmnibox {
  [self.omniboxCoordinator focusOmnibox];
  [self.toolbarConsumer setOmniboxFocused:YES];
  [self.omniboxCoordinator.animatee setClearButtonFaded:NO];
}

- (void)defocusOmnibox {
  [self.omniboxCoordinator endEditing];
  [self.toolbarConsumer setOmniboxFocused:NO];
  [self.omniboxCoordinator.animatee setClearButtonFaded:YES];
}

- (void)goBack {
  if (_historyStack.count < 2) {
    [self updateBackButton];
    return;
  }

  // Remove the current navigation.
  [_historyStack removeLastObject];

  // If the LensResult is different, reload the result.
  HistoryElement* lastEntry = _historyStack.lastObject;
  if (lastEntry.lensResult != _currentLensResult) {
    // When reloading, ignore the URL navigation. URL from lensResult doesn't
    // contains sub navigations (navigations with the same lensResult). The
    // correct URL is loaded below.
    _skipLoadingNextLensResultURL = YES;
    [self.lensHandler reloadResult:lastEntry.lensResult];
  }

  // Reloading the URL will add a new history entry on `didStartNavigation`.
  [_historyStack removeLastObject];
  [self.resultConsumer loadResultsURL:lastEntry.URL];
}

#pragma mark OmniboxFocusDelegate

- (void)omniboxDidBecomeFirstResponder {
  [self focusOmnibox];
}

- (void)omniboxDidResignFirstResponder {
  [self defocusOmnibox];
}

#pragma mark - ChromeLensOverlayDelegate

// The lens overlay started searching for a result.
- (void)lensOverlayDidStartSearchRequest:(id<ChromeLensOverlay>)lensOverlay {
  [self.resultConsumer handleSearchRequestStarted];
}

// The lens overlay search request produced an error.
- (void)lensOverlayDidReceiveError:(id<ChromeLensOverlay>)lensOverlay {
  [self.resultConsumer handleSearchRequestErrored];
}

// The lens overlay search request produced a valid result.
- (void)lensOverlay:(id<ChromeLensOverlay>)lensOverlay
    didGenerateResult:(id<ChromeLensOverlayResult>)result {
  _currentLensResult = result;
  if (!_skipLoadingNextLensResultURL) {
    [self.resultConsumer loadResultsURL:result.searchResultURL];
  }
  _skipLoadingNextLensResultURL = NO;
  [self.omniboxCoordinator setThumbnailImage:result.selectionPreviewImage];
}

- (void)lensOverlayDidTapOnCloseButton:(id<ChromeLensOverlay>)lensOverlay {
  [self.commandsHandler destroyLensUI:YES];
}

- (void)lensOverlay:(id<ChromeLensOverlay>)lensOverlay
    suggestSignalsAvailableOnResult:(id<ChromeLensOverlayResult>)result {
  // TODO(crbug.com/366156296): Implement.
}

#pragma mark - Private

/// Adds the URL navigation to the `historyStack`.
- (void)addURLToHistory:(const GURL&)URL {
  HistoryElement* element = [[HistoryElement alloc] init];
  element.URL = URL;
  element.lensResult = _currentLensResult;
  [_historyStack addObject:element];
  [self updateBackButton];
}

/// Updates the back button availability.
- (void)updateBackButton {
  [self.toolbarConsumer setCanGoBack:_historyStack.count > 1];
}

@end
