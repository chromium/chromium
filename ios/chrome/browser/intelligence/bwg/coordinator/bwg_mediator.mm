// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator.h"

#import <memory>

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

@interface BWGMediator ()

// The base view controller to present UI.
@property(nonatomic, weak) UIViewController* baseViewController;

@end

@implementation BWGMediator {
  // The current web state list.
  raw_ptr<WebStateList> _webStateList;

  // Pref service to check if user flows were previously triggered.
  raw_ptr<PrefService> _prefService;

  // The profile-scoped BWG service.
  raw_ptr<BwgService> _BWGService;

  // The browser-scoped BWG browser agent.
  raw_ptr<BwgBrowserAgent> _BWGBrowserAgent;

  // The PageContext wrapper used to provide context about a page.
  PageContextWrapper* _pageContextWrapper;

  // Start time for the preparation of the presentation of BWG overlay.
  base::TimeTicks _BWGOverlayPreparationStartTime;

  // Whether the FRE was presented for the current BWG instance.
  BOOL _didPresentBWGFRE;

  // The feature engagement tracker.
  raw_ptr<feature_engagement::Tracker> _tracker;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                       webStateList:(WebStateList*)webStateList
                 baseViewController:(UIViewController*)baseViewController
                         BWGService:(BwgService*)BWGService
                    BWGBrowserAgent:(BwgBrowserAgent*)BWGBrowserAgent
                            tracker:(feature_engagement::Tracker*)tracker {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _webStateList = webStateList;
    _baseViewController = baseViewController;
    _BWGService = BWGService;
    _BWGBrowserAgent = BWGBrowserAgent;
    _tracker = tracker;
  }
  return self;
}

- (void)presentBWGFlow {
  _BWGOverlayPreparationStartTime = base::TimeTicks::Now();

  switch (BWGPromoConsentVariationsParam()) {
    case BWGPromoConsentVariations::kSkipConsent:
      [self prepareBWGOverlay];
      return;
    case BWGPromoConsentVariations::kForceFRE:
      // Resetting the consent pref will allow the BWG flow to act as if consent
      // was never given.
      _prefService->SetBoolean(prefs::kIOSBwgConsent, NO);
      break;
    default:
      break;
  }

  _didPresentBWGFRE = [self.delegate maybePresentBWGFRE];
  if (_didPresentBWGFRE) {
    BwgTabHelper* BWGTabHelper = [self activeWebStateBWGTabHelper];
    if (!BWGTabHelper) {
      return;
    }
    BWGTabHelper->SetIsFirstRun(true);
    return;
  }

  [self prepareBWGOverlay];
}

#pragma mark - BWGConsentMutator

// Did consent to BWG.
- (void)didConsentBWG {
  _prefService->SetBoolean(prefs::kIOSBwgConsent, YES);
  if (IsGeminiNavigationPromoEnabled()) {
    _tracker->NotifyEvent(feature_engagement::events::kIOSGeminiConsentGiven);
  }
  __weak __typeof(self) weakSelf = self;
  [_delegate dismissBWGConsentUIWithCompletion:^{
    [weakSelf prepareBWGOverlay];
  }];
}

// Did dismisses the Consent UI.
- (void)didRefuseBWGConsent {
  [_delegate dismissBWGFlow];
}

// Did close BWG Promo UI.
- (void)didCloseBWGPromo {
  [_delegate dismissBWGFlow];
}

// Open a new tab page given a URL.
- (void)openNewTabWithURL:(const GURL&)URL {
  [self FREWillBeBackgrounded];
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.applicationHandler openURLInNewTab:command];
}

#pragma mark - Private

// Prepares BWG overlay.
- (void)prepareBWGOverlay {
  // Cancel any ongoing page context operation.
  if (_pageContextWrapper) {
    _pageContextWrapper = nil;
  }

  if (IsZeroStateSuggestionsAskGeminiEnabled()) {
    [self executeZeroStateSuggestions];
  }

  // Configure the callback to be executed once the page context is ready.
  __weak __typeof(self) weakSelf = self;
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  base::OnceCallback<void(PageContextWrapperCallbackResponse)>
      page_context_completion_callback;
  if (IsGeminiImmediateOverlayEnabled()) {
    // Present the overlay immediately without page context.
    [self openPendingBWGOverlay];

    page_context_completion_callback =
        base::BindOnce(^void(PageContextWrapperCallbackResponse response) {
          [weakSelf updateBWGOverlayForWebState:activeWebState
                     pageContextWrapperResponse:std::move(response)];
        });
  } else {
    page_context_completion_callback =
        base::BindOnce(^void(PageContextWrapperCallbackResponse response) {
          [weakSelf openBWGOverlayForPage:std::move(response)];
        });
  }

  // Collect the PageContext and execute the callback once it's ready.
  _pageContextWrapper = [[PageContextWrapper alloc]
        initWithWebState:activeWebState
      completionCallback:std::move(page_context_completion_callback)];
  [_pageContextWrapper setShouldGetAnnotatedPageContent:YES];
  [_pageContextWrapper setShouldGetSnapshot:YES];
  // Attempt to populate page context fields. If the page is still loading,
  // processing will start once the page has loaded.
  if (IsGeminiImmediateOverlayEnabled() && activeWebState &&
      activeWebState->IsLoading()) {
    BwgTabHelper* BWGTabHelper = [self activeWebStateBWGTabHelper];
    base::OnceCallback<void()> pageContextPopulateCallback =
        base::BindOnce(^void() {
          [weakSelf populatePageContextFieldsAsync];
        });
    if (BWGTabHelper) {
      BWGTabHelper->SetPageLoadedCallback(
          std::move(pageContextPopulateCallback));
    }
  } else {
    [_pageContextWrapper populatePageContextFieldsAsync];
  }
}

// Begins asynchronous work to populate page context fields for the current
// page.
- (void)populatePageContextFieldsAsync {
  if (!_pageContextWrapper) {
    return;
  }
  [_pageContextWrapper populatePageContextFieldsAsync];
}

// Opens the BWG overlay with a given PageContextWrapperCallbackResponse.
- (void)openBWGOverlayForPage:
    (PageContextWrapperCallbackResponse)pageContextWrapperResponse {
  _pageContextWrapper = nil;

  web::WebState* activeWebState = _webStateList->GetActiveWebState();

  // The active web state may no longer be eligible for Gemini by the time this
  // is called. If this is the case, the overlay should not be presented.
  if (!activeWebState ||
      !_BWGService->IsBwgAvailableForWebState(activeWebState)) {
    return;
  }

  _BWGBrowserAgent->PresentBwgOverlay(self.baseViewController,
                                      std::move(pageContextWrapperResponse));

  base::UmaHistogramLongTimes100(
      _didPresentBWGFRE ? kStartupTimeWithFREHistogram
                        : kStartupTimeNoFREHistogram,
      base::TimeTicks::Now() - _BWGOverlayPreparationStartTime);
}

// Opens the BWG overlay in a pending state, since full page context is not yet
// ready.
- (void)openPendingBWGOverlay {
  _pageContextWrapper = nil;

  web::WebState* activeWebState = _webStateList->GetActiveWebState();

  // The active web state may no longer be eligible for Gemini by the time this
  // is called. If this is the case, the overlay should not be presented.
  if (!activeWebState ||
      !_BWGService->IsBwgAvailableForWebState(activeWebState)) {
    return;
  }

  // Set parts of PageContext (i.e. url and title) that are available before the
  // page is done loading.
  std::unique_ptr<optimization_guide::proto::PageContext> partialPageContext =
      std::make_unique<optimization_guide::proto::PageContext>();
  partialPageContext->set_url(activeWebState->GetVisibleURL().spec());
  partialPageContext->set_title(base::UTF16ToUTF8(activeWebState->GetTitle()));

  _BWGBrowserAgent->PresentPendingBwgOverlay(self.baseViewController,
                                             std::move(partialPageContext));

  base::UmaHistogramLongTimes100(
      _didPresentBWGFRE ? kStartupTimeWithFREHistogram
                        : kStartupTimeNoFREHistogram,
      base::TimeTicks::Now() - _BWGOverlayPreparationStartTime);
}

// Updates the BWG overlay with a given PageContextWrapperCallbackResponse.
- (void)updateBWGOverlayForWebState:(web::WebState*)webState
         pageContextWrapperResponse:
             (PageContextWrapperCallbackResponse)response {
  _pageContextWrapper = nil;

  // The original web state may no longer be eligible for Gemini by the time
  // this is called. If this is the case, the overlay should not update.
  if (!webState || !_BWGService->IsBwgAvailableForWebState(webState)) {
    return;
  }

  // The current web state may have changed. If so, do not update the overlay.
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState || activeWebState->GetUniqueIdentifier() !=
                             webState->GetUniqueIdentifier()) {
    return;
  }

  _BWGBrowserAgent->UpdateBwgOverlayPageContext(std::move(response));
}

// Notifies the currently active WebState's BWG tab helper that the FRE will be
// backgrounded.
- (void)FREWillBeBackgrounded {
  BwgTabHelper* BWGTabHelper = [self activeWebStateBWGTabHelper];
  if (!BWGTabHelper) {
    return;
  }

  BWGTabHelper->SetBwgUiShowing(false);
  BWGTabHelper->PrepareBwgFreBackgrounding();
}

// Returns the currently active WebState's BWG tab helper.
- (BwgTabHelper*)activeWebStateBWGTabHelper {
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return nil;
  }

  return BwgTabHelper::FromWebState(activeWebState);
}

// Fetches zero-state suggestions from the BWG tab helper and pass them to the
// UI provider through a callback.
- (void)executeZeroStateSuggestions {
  if (!IsZeroStateSuggestionsAskGeminiEnabled()) {
    return;
  }

  BwgTabHelper* tabHelper = [self activeWebStateBWGTabHelper];
  if (!tabHelper) {
    return;
  }

  tabHelper->ExecuteZeroStateSuggestions(
      base::BindOnce(^(NSArray<NSString*>* suggestions){
          // No-op.
      }));
}

@end
