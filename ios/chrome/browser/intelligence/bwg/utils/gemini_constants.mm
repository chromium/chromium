// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

NSString* const kGeminiPromoConsentFullDetentIdentifier =
    @"GeminiPromoConsentFullDetentIdentifier";

NSString* const kLottieAnimationFirstRunBannerName = @"FRE_Banner";

NSString* const kLottieAnimationFRESummarizeSlideName = @"FRE_Summarize_Slide";
NSString* const kLottieAnimationFRESummarizeSlideRTLName =
    @"FRE_Summarize_Slide_RTL";

NSString* const kLottieAnimationFREShoppingSlideName = @"FRE_Shopping_Slide";
NSString* const kLottieAnimationFREShoppingSlideDarkName =
    @"FRE_Shopping_Slide_Dark";
NSString* const kLottieAnimationFREShoppingSlideRTLName =
    @"FRE_Shopping_Slide_RTL";
NSString* const kLottieAnimationFREShoppingSlideDarkRTLName =
    @"FRE_Shopping_Slide_Dark_RTL";

NSString* const kLottieAnimationFREPlanningSlideName = @"FRE_Planning_Slide";
NSString* const kLottieAnimationFREPlanningSlideDarkName =
    @"FRE_Planning_Slide_Dark";
NSString* const kLottieAnimationFREPlanningSlideRTLName =
    @"FRE_Planning_Slide_RTL";
NSString* const kLottieAnimationFREPlanningSlideDarkRTLName =
    @"FRE_Planning_Slide_Dark_RTL";

NSDictionary<NSString*, UIColor*>* SummarizeSlideLightModeColorProvider() {
  return @{
    @"575B5F" : UIColorFromRGB(0x575B5F),
    @"1B1C1D" : UIColorFromRGB(0x1B1C1D),
    @"F0F4F9" : UIColorFromRGB(0xF0F4F9),
    @"FFFFFF" : UIColorFromRGB(0xFFFFFF),
    @"BDC1C6" : UIColorFromRGB(0xBDC1C6),
    @"A8C7FA" : UIColorFromRGB(0xA8C7FA),
    @"D3E3FD" : UIColorFromRGB(0xD3E3FD),
    @"CEEAD6" : UIColorFromRGB(0xCEEAD6),
    @"AECBFA" : UIColorFromRGB(0xAECBFA),
    @"A8DAB5" : UIColorFromRGB(0xA8DAB5),
    @"E5EEFE" : UIColorFromRGB(0xE5EEFE),
    @"ECF4F9" : UIColorFromRGB(0xECF4F9),
    @"kInvertedTextPrimaryColor" : UIColorFromRGB(0xFFFFFF),
    @"Floaty-text" : [UIColor colorNamed:kTextPrimaryColor],
    @"Tab-text" : [UIColor colorNamed:kTextPrimaryColor],
  };
}

NSDictionary<NSString*, UIColor*>* SummarizeSlideDarkModeColorProvider() {
  return @{
    @"575B5F" : UIColorFromRGB(0xA2A9B0),
    @"1B1C1D" : UIColorFromRGB(0xFFFFFF),
    @"F0F4F9" : UIColorFromRGB(0x282A2C),
    @"FFFFFF" : UIColorFromRGB(0x131314),
    @"BDC1C6" : UIColorFromRGB(0x000000),
    @"A8C7FA" : UIColorFromRGB(0x0B57D0),
    @"D3E3FD" : UIColorFromRGB(0x0842A0),
    @"CEEAD6" : UIColorFromRGB(0x137333),
    @"AECBFA" : UIColorFromRGB(0x0842A0),
    @"A8DAB5" : UIColorFromRGB(0x137333),
    @"E5EEFE" : UIColorFromRGB(0x052A66),
    @"ECF4F9" : UIColorFromRGB(0x072A37),
    @"kInvertedTextPrimaryColor" : UIColorFromRGB(0x000000),
    @"Floaty-text" : [UIColor colorNamed:kTextPrimaryColor],
    @"Tab-text" : [UIColor colorNamed:kTextPrimaryColor],
  };
}

NSString* const kGeminiFRECarouselScrollViewAccessibilityIdentifier =
    @"GeminiFRECarouselScrollViewAccessibilityIdentifier";

const char kLastInteractionTimestampDictKey[] = "last_interaction_timestamp";
const char kURLOnLastInteractionDictKey[] = "url_on_last_interaction";

// Consent row links for the new FRE.
const char kDataGovernanceManagedLinkURL[] =
    "https://support.google.com/a/answer/15706919";
const char kActivityLinkURL[] = "https://myactivity.google.com/product/gemini";
const char kChoicesLinkURL[] =
    "https://support.google.com/gemini/answer/"
    "13594961?visit_id=639210347224714317-2286050145&p=activity_settings&rd=1#"
    "pn_config_settings&zippy=%2Cconfiguring-your-settings";
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
const char kLivePrivacyHubManagedLinkURL[] =
    "https://knowledge.workspace.google.com/admin/generative-ai/"
    "generative-ai-in-google-workspace-privacy-hub";

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
NSString* const kGeminiActivityLinkAction = @"GeminiActivityLinkAction";
NSString* const kGeminiChoicesLinkAction = @"GeminiChoicesLinkAction";
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
NSString* const kGeminiLivePrivacyHubManagedLinkAction =
    @"GeminiLivePrivacyHubManagedLinkAction";

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

// The accessibility ID of the Gemini wrapper stack in the first run flow.
NSString* const kGeminiFirstRunWrapperStackAccessibilityIdentifier =
    @"GeminiFirstRunWrapperStackAccessibilityIdentifier";

@implementation GeminiStartupState

- (instancetype)initWithEntryPoint:(gemini::EntryPoint)entryPoint {
  self = [super init];
  if (self) {
    _entryPoint = entryPoint;
  }
  return self;
}

@end
