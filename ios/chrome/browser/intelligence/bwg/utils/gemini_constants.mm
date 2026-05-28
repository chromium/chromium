// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"

NSString* const kGeminiPromoConsentFullDetentIdentifier =
    @"GeminiPromoConsentFullDetentIdentifier";

NSString* const kLottieAnimationFREBannerName = @"FRE_Banner";

const char kLastInteractionTimestampDictKey[] = "last_interaction_timestamp";
const char kURLOnLastInteractionDictKey[] = "url_on_last_interaction";

// Consent row links for the new FRE.
const char kDataGovernanceManagedLinkURL[] =
    "https://support.google.com/a/answer/15706919";
const char kDataGovernanceStrictLinkURL[] =
    "https://support.google.com/gemini?p=privacy_notice";
const char kDataGovernanceNormalLocationLinkURL[] =
    "https://support.google.com/gemini/answer/"
    "13594961?hl=en#location_info&zippy=%2Cwhat-location-information-do-gemini-"
    "apps-collect-why-and-how-is-it-used";
const char kDataGovernanceNormalChoicesLinkURL[] =
    "https://support.google.com/gemini/answer/"
    "13594961?visit_id=638773303691545173-4156329828&p=activity_settings&rd=1#"
    "config_settings";
const char kConnectedServicesLinkURL[] =
    "https://support.google.com/gemini/answer/13594961";

// Consent row links for the old FRE.
// TODO(crbug.com/393204662): Remove these links once the old FRE is removed.
const char kSecondBoxLinkURLManagedAccount[] =
    "https://support.google.com/a/answer/15706919";
const char kSecondBoxLink1URLNonManagedAccount[] =
    "https://support.google.com/gemini/answer/"
    "13594961?visit_id=638773303691545173-4156329828&p=activity_settings&rd=1#"
    "config_settings";
const char kSecondBoxLink2URLNonManagedAccount[] =
    "https://support.google.com/gemini/answer/"
    "13594961?hl=en#location_info&zippy=%2Cwhat-location-information-do-gemini-"
    "apps-collect-why-and-how-is-it-used";

// Consent row links for Live FRE.
const char kLivePrivacyNoticeLinkURL[] =
    "https://support.google.com/gemini/answer/13594961";
const char kLiveLearnMoreLinkURL[] =
    "https://support.google.com/gemini/answer/13594961";
const char kLivePrivacyPolicyLinkURL[] = "https://policies.google.com/privacy";

// Footnote links.
const char kFirstFootnoteLinkURL[] = "https://policies.google.com/terms";
const char kSecondFootnoteLinkURL[] =
    "https://support.google.com/gemini/answer/13594961";
const char kKoreanTermsFootnoteLinkURL[] =
    "https://www.google.com/intl/ko/policies/terms/location";
const char kWatchLinkURL[] = "https://support.google.com/gemini?p=about_ai";

// Action identifiers for links in the new FRE Gemini consent rows.
NSString* const kGeminiDataGovernanceManagedLinkAction =
    @"GeminiDataGovernanceManagedLinkAction";
NSString* const kGeminiDataGovernanceStrictLinkAction =
    @"GeminiDataGovernanceStrictLinkAction";
NSString* const kGeminiDataGovernanceNormalLocationLinkAction =
    @"GeminiDataGovernanceNormalLocationLinkAction";
NSString* const kGeminiDataGovernanceNormalChoicesLinkAction =
    @"GeminiDataGovernanceNormalChoicesLinkAction";
NSString* const kGeminiConnectedServicesLinkAction =
    @"GeminiConnectedServicesLinkAction";

// Action identifiers for links in the old FRE Gemini consent rows.
NSString* const kGeminiSecondBoxLinkActionManagedAccount =
    @"GeminiSecondBoxLinkActionManagedAccount";
NSString* const kGeminiSecondBoxLink1ActionNonManagedAccount =
    @"GeminiSecondBoxLink1ActionNonManagedAccount";
NSString* const kGeminiSecondBoxLink2ActionNonManagedAccount =
    @"GeminiSecondBoxLink2ActionNonManagedAccount";

// Action identifiers for links in the Live FRE Gemini consent rows.
NSString* const kGeminiLivePrivacyNoticeLinkAction =
    @"GeminiLivePrivacyNoticeLinkAction";
NSString* const kGeminiLiveLearnMoreLinkAction =
    @"GeminiLiveLearnMoreLinkAction";
NSString* const kGeminiLivePrivacyPolicyLinkAction =
    @"GeminiLivePrivacyPolicyLinkAction";

// Action identifier for links in the Gemini consent footnote.
NSString* const kGeminiFirstFootnoteLinkAction =
    @"GeminiFirstFootnoteLinkAction";
NSString* const kGeminiSecondFootnoteLinkAction =
    @"GeminiSecondFootnoteLinkAction";
NSString* const kGeminiKoreanTermsLinkAction = @"GeminiKoreanTermsLinkAction";
NSString* const kGeminiWatchLinkAction = @"GeminiWatchLinkAction";

// Accessibility identifiers for Gemini consent view.
NSString* const kGeminiFootNoteTextViewAccessibilityIdentifier =
    @"GeminiFootNoteTextViewAccessibilityIdentifier";

const int kGeminiContextualCueChipSlidingWindow = 2;

@implementation GeminiStartupState

- (instancetype)initWithEntryPoint:(gemini::EntryPoint)entryPoint {
  self = [super init];
  if (self) {
    _entryPoint = entryPoint;
  }
  return self;
}

@end
