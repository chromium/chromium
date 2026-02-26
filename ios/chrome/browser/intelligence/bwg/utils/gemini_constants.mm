// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"

NSString* const kGeminiPromoConsentFullDetentIdentifier =
    @"GeminiPromoConsentFullDetentIdentifier";

NSString* const kLottieAnimationFREBannerName = @"FRE_Banner";

const char kLastInteractionTimestampDictKey[] = "last_interaction_timestamp";
const char kURLOnLastInteractionDictKey[] = "url_on_last_interaction";

// Links for attributed links.
const char kFirstFootnoteLinkURL[] = "https://policies.google.com/terms";
const char kSecondFootnoteLinkURL[] =
    "https://support.google.com/gemini/answer/13594961";
const char kFootnoteLinkURLManagedAccount[] =
    "https://support.google.com/a/answer/15706919";
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

// Accessibility identifiers for Gemini consent view.
NSString* const kGeminiFootNoteTextViewAccessibilityIdentifier =
    @"GeminiFootNoteTextViewAccessibilityIdentifier";
NSString* const kGeminiPrimaryButtonAccessibilityIdentifier =
    @"GeminiPrimaryButtonAccessibilityIdentifier";
NSString* const kGeminiSecondaryButtonAccessibilityIdentifier =
    @"GeminiSecondaryButtonAccessibilityIdentifier";

// Action identifier on a tap on links in the footnote of the Gemini consent
// view.
NSString* const kGeminiFirstFootnoteLinkAction =
    @"GeminiFirstFootnoteLinkAction";
NSString* const kGeminiSecondFootnoteLinkAction =
    @"GeminiSecondFootnoteLinkAction";
NSString* const kGeminiFootnoteLinkActionManagedAccount =
    @"GeminiFootnoteLinkActionManagedAccount";
NSString* const kGeminiSecondBoxLinkActionManagedAccount =
    @"GeminiSecondBoxLinkActionManagedAccount";
NSString* const kGeminiSecondBoxLink1ActionNonManagedAccount =
    @"GeminiSecondBoxLink1ActionNonManagedAccount";
NSString* const kGeminiSecondBoxLink2ActionNonManagedAccount =
    @"GeminiSecondBoxLink2ActionNonManagedAccount";

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
