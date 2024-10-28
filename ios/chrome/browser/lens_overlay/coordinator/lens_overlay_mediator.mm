// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"

#import <memory>

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
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_navigation_manager.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_navigation_mutator.h"
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
#import "ios/web/public/navigation/referrer.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

@interface LensOverlayMediator () <LensOverlayNavigationMutator,
                                   SearchEngineObserving>

/// Current lens result.
@property(nonatomic, strong, readwrite) id<ChromeLensOverlayResult>
    currentLensResult;
/// Number of tab opened by the lens overlay.
@property(nonatomic, assign, readwrite) NSInteger generatedTabCount;

@end

@implementation LensOverlayMediator {
  /// Whether the browser is off the record.
  BOOL _isIncognito;
  /// Search engine observer.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  /// Orchestrates the navigation in the bottom sheet of the lens result page.
  std::unique_ptr<LensOverlayNavigationManager> _navigationManager;
}

- (instancetype)initWithIsIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _isIncognito = isIncognito;
    _navigationManager = std::make_unique<LensOverlayNavigationManager>(self);
  }
  return self;
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
  _searchEngineObserver.reset();
  _navigationManager.reset();
  _currentLensResult = nil;
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

#pragma mark LensOmniboxClientDelegate

- (void)omniboxDidAcceptText:(const std::u16string&)text
              destinationURL:(const GURL&)destinationURL
            thumbnailRemoved:(BOOL)thumbnailRemoved {
  [self defocusOmnibox];
  // Start new unimodal searches in a new tab.
  if (thumbnailRemoved || _currentLensResult.isTextSelection) {
    [self.delegate lensOverlayMediatorOpenURLInNewTabRequsted:destinationURL];
    [self recordNewTabGeneratedBy:lens::LensOverlayNewTabSource::kOmnibox];
    [self updateForLensResult:_currentLensResult];
  } else {
    // Setting the query text generates new results.
    NSString* nsText = base::SysUTF16ToNSString(text);
    [self updateOmniboxText:nsText];
    [self.lensHandler setQueryText:nsText clearSelection:thumbnailRemoved];
  }
}

#pragma mark LensToolbarMutator

- (void)focusOmnibox {
  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.FocusOmnibox"));
  [self.omniboxCoordinator focusOmnibox];
  [self.toolbarConsumer setOmniboxFocused:YES];
  [self.omniboxCoordinator.animatee setClearButtonFaded:NO];
  [self.presentationDelegate requestMaximizeBottomSheet];
}

- (void)defocusOmnibox {
  [self.omniboxCoordinator endEditing];
  [self.toolbarConsumer setOmniboxFocused:NO];
  [self.omniboxCoordinator.animatee setClearButtonFaded:YES];
}

- (void)goBack {
  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.Back"));
  if (_navigationManager) {
    _navigationManager->GoBack();
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
  if (_navigationManager) {
    _navigationManager->LensOverlayDidGenerateResult(result);
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
  [self setOmniboxSuggestSignals:result];
}

- (void)lensOverlay:(id<ChromeLensOverlay>)lensOverlay
    didRequestToOpenURL:(GURL)URL {
  [self.resultConsumer loadResultsURL:URL];
}

- (void)lensOverlayDidOpenOverlayMenu:(id<ChromeLensOverlay>)lensOverlay {
  [self.delegate lensOverlayMediatorDidOpenOverlayMenu:self];
}

#pragma mark - LensOverlayNavigationMutator

- (void)loadLensResult:(id<ChromeLensOverlayResult>)result {
  _currentLensResult = result;
  // Load the URL, it will start the result UI.
  [self.resultConsumer loadResultsURL:result.searchResultURL];
  [self updateForLensResult:result];
}

- (void)reloadLensResult:(id<ChromeLensOverlayResult>)result {
  // Pre update the UI.
  [self updateForLensResult:result];
  // Reload the result.
  [self.lensHandler reloadResult:result];
}

- (void)reloadURL:(GURL)URL {
  [self.resultConsumer loadResultsURL:URL];
}

- (void)onBackNavigationAvailabilityMaybeChanged:(BOOL)canGoBack {
  [self.toolbarConsumer setCanGoBack:canGoBack];
}

#pragma mark - LensResultPageMediatorDelegate

- (void)lensResultPageWebStateDestroyed {
  [self.commandsHandler
      destroyLensUI:YES
             reason:lens::LensOverlayDismissalSource::kTabClosed];
}

- (void)lensResultPageDidChangeActiveWebState:(web::WebState*)webState {
  if (_navigationManager) {
    _navigationManager->SetWebState(webState);
  }
}

- (void)lensResultPageMediator:(LensResultPageMediator*)mediator
       didOpenNewTabFromSource:(lens::LensOverlayNewTabSource)newTabSource {
  [self recordNewTabGeneratedBy:newTabSource];
}

- (void)lensResultPageOpenURLInNewTabRequsted:(GURL)URL {
  [self.delegate lensOverlayMediatorOpenURLInNewTabRequsted:URL];
}

#pragma mark - Private

/// Updates the UI for lens `result`.
- (void)updateForLensResult:(id<ChromeLensOverlayResult>)result {
  [self.omniboxCoordinator
      setThumbnailImage:result.isTextSelection ? nil
                                               : result.selectionPreviewImage];
  if (self.omniboxClient) {
    [self setOmniboxSuggestSignals:result];
    self.omniboxClient->SetLensResultHasThumbnail(!result.isTextSelection);
  }
  [self updateOmniboxText:result.queryText];
}

/// Updates the steady state omnibox text.
- (void)updateOmniboxText:(NSString*)text {
  if (self.omniboxClient) {
    self.omniboxClient->SetOmniboxSteadyStateText(text);
  }
  [self.omniboxCoordinator updateOmniboxState];
}

/// Sets the omnibox suggest signals with `result`.
- (void)setOmniboxSuggestSignals:(id<ChromeLensOverlayResult>)result {
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

  if (!encodedString.empty()) {
    lens::proto::LensOverlaySuggestInputs response;
    response.set_encoded_image_signals(encodedString);
    self.omniboxClient->SetLensOverlaySuggestInputs(response);
  }
}

/// Records lens overlay opening a new tab.
- (void)recordNewTabGeneratedBy:(lens::LensOverlayNewTabSource)newTabSource {
  self.generatedTabCount += 1;
  lens::RecordNewTabGenerated(newTabSource);
}

@end
