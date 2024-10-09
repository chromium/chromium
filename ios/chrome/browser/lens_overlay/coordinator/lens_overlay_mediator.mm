// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"

#import <memory>
#import <stack>

#import "base/base64url.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/lens/lens_overlay_metrics.h"
#import "components/lens/proto/server/lens_overlay_response.pb.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator_delegate.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_toolbar_consumer.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/edit_view_animatee.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
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

/// Current lens result.
@property(nonatomic, strong, readwrite) id<ChromeLensOverlayResult>
    currentLensResult;
/// Number of tab opened by the lens overlay.
@property(nonatomic, assign, readwrite) NSInteger generatedTabCount;

@end

@implementation LensOverlayMediator {
  /// Whether the browser is off the record.
  BOOL _isIncognito;
  /// Bridges C++ WebStateObserver methods to this mediator.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  /// Search engine observer.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;

  /// History stack for back navigation.
  NSMutableArray<HistoryElement*>* _historyStack;
  /// Whether the next navigation is a reload.
  BOOL _isReloading;
}

- (instancetype)initWithIsIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _isIncognito = isIncognito;
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
  _isReloading = NO;
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  BOOL isLensAvailable =
      search_engines::SupportsSearchImageWithLens(_templateURLService);
  if (!isLensAvailable) {
    [self.commandsHandler destroyLensUI:YES
                                 reason:lens::LensOverlayDismissalSource::
                                            kDefaultSearchEngineChange];
  }
}

#pragma mark - Omnibox

#pragma mark CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  if (navigationContext && !navigationContext->IsSameDocument()) {
    const GURL& URL = navigationContext->GetUrl();
    if ([self shouldAddURLToHistory:URL]) {
      [self addURLToHistory:URL];
    }
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
  // Start new unimodal searches in a new tab.
  if (thumbnailRemoved || _currentLensResult.isTextSelection) {
    OpenNewTabCommand* command =
        [[OpenNewTabCommand alloc] initWithURL:destinationURL
                                      referrer:web::Referrer()
                                   inIncognito:_isIncognito
                                  inBackground:NO
                                      appendTo:OpenPosition::kCurrentTab];
    [self.applicationHandler openURLInNewTab:command];
    [self recordNewTabGeneratedBy:lens::LensOverlayNewTabSource::kOmnibox];
    [self resetOmniboxToCurrentLensResult];
  } else {
    // Setting the query text generates new results.
    [self.lensHandler setQueryText:base::SysUTF16ToNSString(text)
                    clearSelection:thumbnailRemoved];
  }
}

#pragma mark LensToolbarMutator

- (void)focusOmnibox {
  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.FocusOmnibox"));
  [self.omniboxCoordinator focusOmnibox];
  [self.toolbarConsumer setOmniboxFocused:YES];
  [self.omniboxCoordinator.animatee setClearButtonFaded:NO];
  [self.presentationDelegate restrictSheetToLargeDetent:YES];
}

- (void)defocusOmnibox {
  [self.omniboxCoordinator endEditing];
  [self.toolbarConsumer setOmniboxFocused:NO];
  [self.omniboxCoordinator.animatee setClearButtonFaded:YES];
  [self.presentationDelegate restrictSheetToLargeDetent:NO];
}

- (void)goBack {
  if (_historyStack.count < 2) {
    [self updateBackButton];
    return;
  }

  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.Back"));

  // Remove the current navigation.
  [_historyStack removeLastObject];

  // If the LensResult is different, reload the result.
  HistoryElement* lastEntry = _historyStack.lastObject;
  if (lastEntry.lensResult != _currentLensResult) {
    _isReloading = YES;
    [self.lensHandler reloadResult:lastEntry.lensResult];
  }
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
  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.NewResult"));
  _currentLensResult = result;
  // When reloading, replace the last object.
  if (_isReloading) {
    [_historyStack removeLastObject];
    _isReloading = NO;
  }
  [self.resultConsumer loadResultsURL:result.searchResultURL];

  if (result.isTextSelection) {
    [self.omniboxCoordinator setThumbnailImage:nil];
  } else {
    [self.omniboxCoordinator setThumbnailImage:result.selectionPreviewImage];
  }
  if (self.omniboxClient) {
    self.omniboxClient->SetLensOverlaySuggestInputs(std::nullopt);
  }
}

- (void)lensOverlayDidTapOnCloseButton:(id<ChromeLensOverlay>)lensOverlay {
  [self.commandsHandler
      destroyLensUI:YES
             reason:lens::LensOverlayDismissalSource::kOverlayCloseButton];
}

- (void)lensOverlay:(id<ChromeLensOverlay>)lensOverlay
    suggestSignalsAvailableOnResult:(id<ChromeLensOverlayResult>)result {
  if (result != _currentLensResult) {
    return;
  }

  // Push the suggest signals to the client.
  if (!self.omniboxClient) {
    return;
  }

  NSData* data = result.suggestSignals;
  if (!data.length) {
    self.omniboxClient->SetLensOverlaySuggestInputs(std::nullopt);
    return;
  }
  std::string encodedString;
  base::span<const uint8_t> signals = base::span<const uint8_t>(
      static_cast<const uint8_t*>(result.suggestSignals.bytes),
      result.suggestSignals.length);

  Base64UrlEncode(signals, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                  &encodedString);

  if (encodedString.size() > 0) {
    lens::proto::LensOverlaySuggestInputs response;
    response.set_encoded_image_signals(encodedString);
    self.omniboxClient->SetLensOverlaySuggestInputs(response);
  }
}

- (void)lensOverlay:(id<ChromeLensOverlay>)lensOverlay
    didRequestToOpenURL:(GURL)URL {
  [self.resultConsumer loadResultsURL:URL];
}

- (void)lensOverlayDidOpenOverlayMenu:(id<ChromeLensOverlay>)lensOverlay {
  [self.delegate lensOverlayMediatorDidOpenOverlayMenu:self];
}

#pragma mark - LensResultPageMediatorDelegate

- (void)lensResultPageWebStateDestroyed {
  [self.commandsHandler
      destroyLensUI:YES
             reason:lens::LensOverlayDismissalSource::kTabClosed];
}

- (void)lensResultPageDidChangeActiveWebState:(web::WebState*)webState {
  self.webState = webState;
}

- (void)lensResultPageMediator:(LensResultPageMediator*)mediator
       didOpenNewTabFromSource:(lens::LensOverlayNewTabSource)newTabSource {
  [self recordNewTabGeneratedBy:newTabSource];
}

#pragma mark - Private

/// Resets the omnibox state to the `_currentLensResult` text and thumbnail.
- (void)resetOmniboxToCurrentLensResult {
  [self.omniboxCoordinator updateOmniboxState];
  [self.omniboxCoordinator
      setThumbnailImage:_currentLensResult.selectionPreviewImage];
}

/// Whether the navigation to `URL` with the `_currentLensResult` should be
/// added to the history stack.
- (BOOL)shouldAddURLToHistory:(const GURL&)URL {
  if (!_historyStack.count) {
    return YES;
  }

  // TODO(crbug.com/370708965): Add sub-navigation support for the latest
  // result. When supporting sub-navigation. Don't add the
  // URL to the history stack if the only change is dark/light mode.

  // Only add lens result change to the history.
  return _historyStack.lastObject.lensResult != _currentLensResult;
}

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

/// Records lens overlay opening a new tab.
- (void)recordNewTabGeneratedBy:(lens::LensOverlayNewTabSource)newTabSource {
  self.generatedTabCount += 1;
  lens::RecordNewTabGenerated(newTabSource);
}

@end
