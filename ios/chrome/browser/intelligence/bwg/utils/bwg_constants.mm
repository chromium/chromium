// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"

NSString* const kBWGPromoConsentFullDetentIdentifier =
    @"BWGPromoConsentFullDetentIdentifier";

NSString* const kLottieAnimationFREBannerName = @"FRE_Banner";

const char kLastInteractionTimestampDictKey[] = "last_interaction_timestamp";
const char kServerIDDictKey[] = "server_id";
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

// Accessibility identifiers for bwg consent view.
NSString* const kBwgFootNoteTextViewAccessibilityIdentifier =
    @"footnote_text_view";
NSString* const kBwgPrimaryButtonAccessibilityIdentifier = @"primary_button";
NSString* const kBwgSecondaryButtonAccessibilityIdentifier =
    @"secondary_button";

// Action identifier on a tap on links in the footnote of the bwg consent view.
NSString* const kBwgFirstFootnoteLinkAction = @"firstFootnoteLinkAction";
NSString* const kBwgSecondFootnoteLinkAction = @"secondFootnoteLinkAction";
NSString* const kBwgFootnoteLinkActionManagedAccount =
    @"footnoteLinkActionManagedAccount";
NSString* const kBwgSecondBoxLinkActionManagedAccount =
    @"secondBoxLinkActionManagedAccount";
NSString* const kBwgSecondBoxLink1ActionNonManagedAccount =
    @"secondBoxLink1ActionNonManagedAccount";
NSString* const kBwgSecondBoxLink2ActionNonManagedAccount =
    @"secondBoxLink2ActionNonManagedAccount";

const int kGeminiContextualCueChipSlidingWindow = 2;
