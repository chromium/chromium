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
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
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
  raw_ptr<GeminiBrowserAgent> _geminiBrowserAgent;

  // Start time for the preparation of the presentation of BWG overlay.
  base::TimeTicks _BWGOverlayPreparationStartTime;

  // Whether the FRE was presented for the current BWG instance.
  BOOL _didPresentBWGFRE;

  // The feature engagement tracker.
  raw_ptr<feature_engagement::Tracker> _tracker;

  // The entry point BWG was started from.
  gemini::EntryPoint _entryPoint;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                       webStateList:(WebStateList*)webStateList
                 baseViewController:(UIViewController*)baseViewController
                         entryPoint:(gemini::EntryPoint)entryPoint
                         BWGService:(BwgService*)BWGService
                 geminiBrowserAgent:(GeminiBrowserAgent*)geminiBrowserAgent
                            tracker:(feature_engagement::Tracker*)tracker {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _webStateList = webStateList;
    _baseViewController = baseViewController;
    _BWGService = BWGService;
    _geminiBrowserAgent = geminiBrowserAgent;
    _tracker = tracker;
    _entryPoint = entryPoint;
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

#pragma mark - GeminiConsentMutator

// Did consent to Gemini.
- (void)didConsentGemini {
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
- (void)didRefuseGeminiConsent {
  [_delegate dismissBWGFlow];
}

// Did close Gemini Promo UI.
- (void)didCloseGeminiPromo {
  [_delegate dismissBWGFlow];
}

// Open a new tab page given a URL.
- (void)openNewTabWithURL:(const GURL&)URL {
  [self FREWillBeBackgrounded];
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.sceneHandler openURLInNewTab:command];
}

// Promo was shown.
- (void)didShowGeminiPromo {
  // No-op.
}

#pragma mark - Private

// Prepares BWG overlay.
- (void)prepareBWGOverlay {
  if (IsZeroStateSuggestionsAskGeminiEnabled()) {
    [self executeZeroStateSuggestions];
  }

  // Configure the callback to be executed once the page context is ready.
  __weak __typeof(self) weakSelf = self;
  web::WebState* activeWebState = _webStateList->GetActiveWebState();

  // Present the overlay immediately without page context.
  [self openPendingBWGOverlay];

  base::RepeatingCallback<void(PageContextWrapperCallbackResponse)>
      page_context_completion_callback = base::BindRepeating(
          ^void(PageContextWrapperCallbackResponse response) {
            [weakSelf updateBWGOverlayForWebState:activeWebState
                       pageContextWrapperResponse:std::move(response)];
          });

  BwgTabHelper* BWGTabHelper = [self activeWebStateBWGTabHelper];
  if (!BWGTabHelper) {
    return;
  }

  BWGTabHelper->SetupPageContextGeneration(
      std::move(page_context_completion_callback));
}

// Opens the BWG overlay in a pending state, since full page context is not yet
// ready.
- (void)openPendingBWGOverlay {

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

  _geminiBrowserAgent->PresentFloatyWithPendingContext(
      self.baseViewController, std::move(partialPageContext),
      [[GeminiStartupState alloc] initWithEntryPoint:_entryPoint]);

  base::UmaHistogramLongTimes100(
      _didPresentBWGFRE ? kStartupTimeWithFREHistogram
                        : kStartupTimeNoFREHistogram,
      base::TimeTicks::Now() - _BWGOverlayPreparationStartTime);
}

// Updates the BWG overlay with a given PageContextWrapperCallbackResponse.
- (void)updateBWGOverlayForWebState:(web::WebState*)webState
         pageContextWrapperResponse:
             (PageContextWrapperCallbackResponse)response {

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

  _geminiBrowserAgent->UpdateFloatyPageContext(std::move(response));
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
