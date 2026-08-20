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
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
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
    void (^completion)(BOOL) = _FRECompletion;
    _FRECompletion = nil;
    completion(NO);
  }
}

- (BOOL)shouldShowPromo {
  BOOL promoImpressionsExhausted =
      _prefService->GetInteger(prefs::kIOSBWGPromoImpressionCount) >=
      kPromoMaxImpressionCount;

  return ShouldForceBWGPromo() || !promoImpressionsExhausted;
}

- (GeminiConsentConfiguration*)consentConfigurationForFirstRunType:
    (GeminiFirstRunType)firstRunType {
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
                         type:firstRunType
                      country:nsCountry];
}

- (BOOL)shouldShowPromoForFirstRunType:(GeminiFirstRunType)firstRunType {
  return self.shouldShowPromo && (firstRunType != GeminiFirstRunType::kLive);
}

- (BOOL)shouldShowBrandingHeaderForFirstRunType:
    (GeminiFirstRunType)firstRunType {
  return !IsGeminiVisualRichFREEnabled() &&
         firstRunType != GeminiFirstRunType::kLive;
}

- (std::vector<GeminiFirstRunStepIdentifier>)stepsForFirstRunType:
    (GeminiFirstRunType)firstRunType {
  // Visual rich and Lightweight first run experiment variants are single-step
  // onboarding flows that are not used for the live entry point.
  if (firstRunType != GeminiFirstRunType::kLive) {
    if (IsGeminiVisualRichFREEnabled()) {
      return {GeminiFirstRunStepIdentifier::kVisualRich};
    }
    if (IsGeminiLightweightFREEnabled()) {
      return {GeminiFirstRunStepIdentifier::kLightweight};
    }
  }
  // Using std::vector to avoid boxing C++ enum class values into NSNumber.
  std::vector<GeminiFirstRunStepIdentifier> steps;
  if ([self shouldShowPromoForFirstRunType:firstRunType]) {
    steps.push_back(GeminiFirstRunStepIdentifier::kPromo);
  }
  steps.push_back(GeminiFirstRunStepIdentifier::kConsent);
  return steps;
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
  return gemini::IsFeatureAvailable(gemini::Feature::kImageRemix,
                                    _identityManager);
}

- (NSString*)lightweightPromoTitle {
  int titleStringID;
  switch (GetGeminiLightweightFREVariant()) {
    case GeminiLightweightFREVariant::kPageSharing:
      titleStringID = IDS_IOS_BWG_LIGHTWEIGHT_PROMO_PAGE_SHARING_TITLE;
      break;
    case GeminiLightweightFREVariant::kDiverse:
      titleStringID = IDS_IOS_BWG_LIGHTWEIGHT_PROMO_DIVERSE_TITLE;
      break;
    case GeminiLightweightFREVariant::kConvenience:
      titleStringID = IDS_IOS_BWG_LIGHTWEIGHT_PROMO_CONVENIENCE_TITLE;
      break;
  }
  return l10n_util::GetNSString(titleStringID);
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
  gemini::UpdateUserConsentToLivePrefs(YES, _prefService);
  __weak __typeof(self) weakSelf = self;
  [_delegate dismissGeminiConsentUIWithCompletion:^{
    [weakSelf handleFRECompletion:YES];
  }];
}

// Did refuse Gemini consent.
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

// Did refuse Live onboarding.
- (void)didRefuseLiveOnboarding {
  // Retain self to survive synchronous teardown from the delegate.
  __strong __typeof(self) strongSelf = self;
  [_delegate dismissGeminiConsentUIWithCompletion:^{
    [strongSelf handleFRECompletion:NO];
  }];
}

// Promo was shown.
- (void)didShowGeminiPromo {
  if (_entryPoint != gemini::EntryPoint::Promo) {
    _tracker->NotifyEvent(
        feature_engagement::events::kIOSGeminiFlowStartedNonPromo);
  }

  [self logPromoShown];
}

- (void)handleFRECompletion:(BOOL)success {
  if (_FRECompletion) {
    void (^completion)(BOOL) = _FRECompletion;
    _FRECompletion = nil;
    completion(success);
  }
}

// Handles tap on a consent link action.
- (void)didTapConsentLinkWithAction:(NSString*)actionString {
  RecordFirstRunConsentAction(IOSGeminiFirstRunAction::kLinkClick);
  if ([actionString isEqualToString:kGeminiFirstFootnoteLinkAction]) {
    [self openNewTabWithURL:GURL(kFirstFootnoteLinkURL)];
  } else if ([actionString isEqualToString:kGeminiSecondFootnoteLinkAction]) {
    [self openNewTabWithURL:GURL(kSecondFootnoteLinkURL)];
  } else if ([actionString
                 isEqualToString:kGeminiSecondBoxLinkActionManagedAccount]) {
    [self openNewTabWithURL:GURL(kSecondBoxLinkURLManagedAccount)];
  } else if ([actionString isEqualToString:
                               kGeminiSecondBoxLink1ActionNonManagedAccount]) {
    [self openNewTabWithURL:GURL(kSecondBoxLink1URLNonManagedAccount)];
  } else if ([actionString isEqualToString:
                               kGeminiSecondBoxLink2ActionNonManagedAccount]) {
    [self openNewTabWithURL:GURL(kSecondBoxLink2URLNonManagedAccount)];
  } else if ([actionString
                 isEqualToString:kGeminiLivePrivacyNoticeLinkAction]) {
    [self openNewTabWithURL:GURL(kLivePrivacyNoticeLinkURL)];
  } else if ([actionString isEqualToString:kGeminiLiveLearnMoreLinkAction]) {
    [self openNewTabWithURL:GURL(kLiveLearnMoreLinkURL)];
  } else if ([actionString
                 isEqualToString:kGeminiLivePrivacyPolicyLinkAction]) {
    [self openNewTabWithURL:GURL(kLivePrivacyPolicyLinkURL)];
  } else if ([actionString
                 isEqualToString:kGeminiLivePrivacyHubManagedLinkAction]) {
    [self openNewTabWithURL:GURL(kLivePrivacyHubManagedLinkURL)];
  } else if ([actionString isEqualToString:kGeminiKoreanTermsLinkAction]) {
    [self openNewTabWithURL:GURL(kKoreanTermsFootnoteLinkURL)];
  } else if ([actionString isEqualToString:kGeminiWatchLinkAction]) {
    [self openNewTabWithURL:GURL(kWatchLinkURL)];
  } else if ([actionString
                 isEqualToString:kGeminiDataGovernanceManagedLinkAction]) {
    [self openNewTabWithURL:GURL(kDataGovernanceManagedLinkURL)];
  } else if ([actionString isEqualToString:kGeminiActivityLinkAction]) {
    [self openNewTabWithURL:GURL(kActivityLinkURL)];
  } else if ([actionString isEqualToString:kGeminiChoicesLinkAction]) {
    [self openNewTabWithURL:GURL(kChoicesLinkURL)];
  } else if ([actionString
                 isEqualToString:kGeminiConnectedServicesLinkAction]) {
    [self openNewTabWithURL:GURL(kConnectedServicesLinkURL)];
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
