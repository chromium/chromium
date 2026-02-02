// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_first_run_mediator.h"

#import <memory>

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_first_run_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
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

namespace {

// The max number of times the promo page should be shown.
const CGFloat kPromoMaxImpressionCount = 3;

}  // namespace

@interface GeminiFirstRunMediator ()

// The base view controller to present UI.
@property(nonatomic, weak) UIViewController* baseViewController;

@end

@implementation GeminiFirstRunMediator {
  // The current web state list.
  raw_ptr<WebStateList> _webStateList;

  // Pref service to check if user flows were previously triggered.
  raw_ptr<PrefService> _prefService;

  // The profile-scoped Gemini service.
  raw_ptr<BwgService> _geminiService;

  // The browser-scoped Gemini browser agent.
  raw_ptr<GeminiBrowserAgent> _geminiBrowserAgent;

  // Start time for the preparation of the presentation of BWG overlay.
  base::TimeTicks _geminiOverlayPreparationStartTime;

  // The feature engagement tracker.
  raw_ptr<feature_engagement::Tracker> _tracker;

  // Completion block for the FRE flow.
  void (^_FRECompletion)(BOOL success);

  // The entry point the mediator was initialized from.
  gemini::EntryPoint _entryPoint;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                       webStateList:(WebStateList*)webStateList
                 baseViewController:(UIViewController*)baseViewController
                         BWGService:(BwgService*)geminiService
                 geminiBrowserAgent:(GeminiBrowserAgent*)geminiBrowserAgent
                            tracker:(feature_engagement::Tracker*)tracker
                         entryPoint:(gemini::EntryPoint)entryPoint
                  completionHandler:(void (^)(BOOL success))completion {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _webStateList = webStateList;
    _tracker = tracker;
    _entryPoint = entryPoint;
    _FRECompletion = completion;
    _geminiOverlayPreparationStartTime = base::TimeTicks::Now();
  }
  return self;
}

- (void)disconnect {
  if (_FRECompletion) {
    _FRECompletion(NO);
    _FRECompletion = nil;
  }
}

- (BOOL)shouldShowPromo {
  BOOL promoImpressionsExhausted =
      _prefService->GetInteger(prefs::kIOSBWGPromoImpressionCount) >=
      kPromoMaxImpressionCount;

  return ShouldForceBWGPromo() || !promoImpressionsExhausted;
}

#pragma mark - Private

- (void)logPromoShown {
  if (IsGeminiNavigationPromoEnabled() &&
      _entryPoint == gemini::EntryPoint::Promo) {
    _tracker->NotifyEvent(
        feature_engagement::events::kIOSFullscreenPromosGroupTrigger);
    _tracker->NotifyEvent(
        feature_engagement::events::kIOSGeminiFullscreenPromoTriggered);
  }
  int impressionCount =
      _prefService->GetInteger(prefs::kIOSBWGPromoImpressionCount) + 1;
  _prefService->SetInteger(prefs::kIOSBWGPromoImpressionCount, impressionCount);

  if (impressionCount == 1) {
    _tracker->NotifyEvent(
        feature_engagement::events::kIOSGeminiPromoFirstCompletion);
    BwgTabHelper* geminiTabHelper = [self activeWebStateGeminiTabHelper];
    if (geminiTabHelper) {
      geminiTabHelper->SetPreventContextualPanelEntryPoint(
          [self shouldShowAIHubIPH]);
    }
  }
}

// Returns whether to show AI Hub IPH.
- (BOOL)shouldShowAIHubIPH {
  BOOL wouldTriggerIPH =
      _tracker->WouldTriggerHelpUI(feature_engagement::kIPHIOSPageActionMenu);

  return _entryPoint != gemini::EntryPoint::AIHub && [self shouldShowPromo] &&
         wouldTriggerIPH;
}

#pragma mark - GeminiConsentMutator

// Did consent to Gemini.
- (void)didConsentGemini {
  _prefService->SetBoolean(prefs::kIOSBwgConsent, YES);
  if (IsGeminiNavigationPromoEnabled()) {
    _tracker->NotifyEvent(feature_engagement::events::kIOSGeminiConsentGiven);
  }
  __weak __typeof(self) weakSelf = self;
  [_delegate dismissGeminiConsentUIWithCompletion:^{
    [weakSelf handleFRECompletion:YES];
  }];
}

// Did dismisses the Consent UI.
- (void)didRefuseGeminiConsent {
  [_delegate dismissGeminiFlow];
  [self handleFRECompletion:NO];
}

// Did close Gemini Promo UI.
- (void)didCloseGeminiPromo {
  [_delegate dismissGeminiFlow];
  [self handleFRECompletion:NO];
}

// Promo was shown.
- (void)didShowGeminiPromo {
  if (_entryPoint != gemini::EntryPoint::Promo) {
    _tracker->NotifyEvent(
        feature_engagement::events::kIOSGeminiFlowStartedNonPromo);
  }

  [self logPromoShown];

  BwgTabHelper* geminiTabHelper = [self activeWebStateGeminiTabHelper];
  if (geminiTabHelper) {
    geminiTabHelper->SetIsFirstRun(true);
  }
}

- (void)handleFRECompletion:(BOOL)success {
  if (_FRECompletion) {
    _FRECompletion(success);
    _FRECompletion = nil;
  }
}

// Open a new tab page given a URL.
- (void)openNewTabWithURL:(const GURL&)URL {
  [self prepareFREBackground];
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.sceneHandler openURLInNewTab:command];
}

// Notifies the currently active WebState's BWG tab helper that the FRE will be
// backgrounded.
- (void)prepareFREBackground {
  BwgTabHelper* geminiTabHelper = [self activeWebStateGeminiTabHelper];
  if (!geminiTabHelper) {
    return;
  }

  geminiTabHelper->SetBwgUiShowing(false);
  geminiTabHelper->PrepareBwgFreBackgrounding();
}

// Returns the currently active WebState's Gemini tab helper.
- (BwgTabHelper*)activeWebStateGeminiTabHelper {
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return nil;
  }

  return BwgTabHelper::FromWebState(activeWebState);
}

@end
