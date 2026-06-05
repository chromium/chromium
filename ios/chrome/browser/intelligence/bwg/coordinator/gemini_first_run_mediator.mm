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
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_first_run_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_feature_availability.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
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
  raw_ptr<GeminiService> _geminiService;

  // Start time for the preparation of the presentation of BWG overlay.
  base::TimeTicks _geminiOverlayPreparationStartTime;

  // The feature engagement tracker.
  raw_ptr<feature_engagement::Tracker> _tracker;

  // Completion block for the FRE flow.
  void (^_FRECompletion)(BOOL success);

  // The identity manager.
  raw_ptr<signin::IdentityManager> _identityManager;

  // The authentication service.
  raw_ptr<AuthenticationService> _authService;

  // The entry point the mediator was initialized from.
  gemini::EntryPoint _entryPoint;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                       webStateList:(WebStateList*)webStateList
                 baseViewController:(UIViewController*)baseViewController
                      geminiService:(GeminiService*)geminiService
              authenticationService:(AuthenticationService*)authService
                    identityManager:(signin::IdentityManager*)identityManager
                            tracker:(feature_engagement::Tracker*)tracker
                         entryPoint:(gemini::EntryPoint)entryPoint
                  completionHandler:(void (^)(BOOL success))completion {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _webStateList = webStateList;
    _geminiService = geminiService;
    _authService = authService;
    _tracker = tracker;
    _entryPoint = entryPoint;
    _FRECompletion = completion;
    _identityManager = identityManager;
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

- (GeminiConsentConfiguration*)consentConfigurationForFREType:
    (GeminiFREType)FREType {
  variations::VariationsService* variationsService =
      GetApplicationContext()->GetVariationsService();
  std::string country =
      variationsService
          ? base::ToLowerASCII(variationsService->GetStoredPermanentCountry())
          : "";
  NSString* nsCountry = base::SysUTF8ToNSString(country);

  BOOL isManagedAccount =
      _authService && _authService->HasPrimaryIdentityManaged();
  return [GeminiConsentConfiguration
      configurationForManaged:isManagedAccount
                       strict:[self useStrictLegalConsent]
                         type:FREType
                      country:nsCountry];
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
    GeminiTabHelper* geminiTabHelper = [self activeWebStateGeminiTabHelper];
    if (geminiTabHelper) {
      geminiTabHelper->SetPreventContextualPanelEntryPoint(
          [self shouldShowAIHubIPH]);
    }
  }
}

// Returns whether to show AI Hub IPH.
- (BOOL)shouldShowAIHubIPH {
  if (IsChromeNextIaEnabled()) {
    return NO;
  }

  BOOL wouldTriggerIPH =
      _tracker->WouldTriggerHelpUI(feature_engagement::kIPHIOSPageActionMenu);

  return _entryPoint != gemini::EntryPoint::AIHub && [self shouldShowPromo] &&
         wouldTriggerIPH;
}

// Returns whether the UI must enforce strict legal consent requirements.
- (BOOL)useStrictLegalConsent {
  return !_geminiService->HasModelExecutionCapability();
}

#pragma mark - GeminiFirstRunMutator

- (BOOL)shouldShowImageRemixRow {
  return IsGeminiImageRemixToolShowFRERowEnabled() &&
         gemini::IsFeatureAvailable(gemini::Feature::kImageRemix,
                                    _identityManager);
}

// Did consent to Gemini.
- (void)didConsentGemini {
  gemini::UpdateUserConsentPrefs(YES, _prefService);
  if (IsGeminiNavigationPromoEnabled()) {
    _tracker->NotifyEvent(feature_engagement::events::kIOSGeminiConsentGiven);
  }
  __weak __typeof(self) weakSelf = self;
  [_delegate dismissGeminiConsentUIWithCompletion:^{
    [weakSelf handleFRECompletion:YES];
  }];
}

// Did consent to Live Gemini.
- (void)didConsentToLiveGemini {
  gemini::UpdateUserConsentPrefs(YES, _prefService);
  __weak __typeof(self) weakSelf = self;
  [_delegate dismissGeminiConsentUIWithCompletion:^{
    [weakSelf handleFRECompletion:YES];
  }];
}

// Did dismiss the Consent UI.
- (void)didRefuseGeminiConsent {
  // Retain self to survive synchronous teardown from the delegate.
  __strong __typeof(self) strongSelf = self;
  gemini::UpdateUserConsentPrefs(NO, _prefService);
  [_delegate dismissGeminiFlow];
  [strongSelf handleFRECompletion:NO];
}

// Did close Gemini Promo UI.
- (void)didCloseGeminiPromo {
  // Retain self to survive synchronous teardown from the delegate.
  __strong __typeof(self) strongSelf = self;
  [_delegate dismissGeminiFlow];
  [strongSelf handleFRECompletion:NO];
}

- (void)didRefuseLiveMicPermission {
  // Retain self to survive synchronous teardown from the delegate.
  __strong __typeof(self) strongSelf = self;
  [_delegate dismissGeminiFlow];
  [strongSelf handleFRECompletion:NO];
}

// Promo was shown.
- (void)didShowGeminiPromo {
  if (_entryPoint != gemini::EntryPoint::Promo) {
    _tracker->NotifyEvent(
        feature_engagement::events::kIOSGeminiFlowStartedNonPromo);
  }

  [self logPromoShown];

  GeminiTabHelper* geminiTabHelper = [self activeWebStateGeminiTabHelper];
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
  [_delegate dismissGeminiFlow];
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.sceneHandler openURLInNewTab:command];
}

// Returns the currently active WebState's Gemini tab helper.
- (GeminiTabHelper*)activeWebStateGeminiTabHelper {
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return nil;
  }

  return GeminiTabHelper::FromWebState(activeWebState);
}

@end
