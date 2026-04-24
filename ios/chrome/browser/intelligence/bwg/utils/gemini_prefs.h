// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_PREFS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_PREFS_H_

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"

class PrefService;

namespace gemini {
// TODO(crbug.com/485892276): Move ProfilePrefs registration here.

// Enterprise policies allow for Gemini usage within Chrome.
bool GeminiAllowedByPolicy(PrefService* prefs);
bool GenAiAllowedByEnterprise(PrefService* prefs);
bool GeminiAllowedByEnterprise(PrefService* prefs);

// Functions that interact with the Gemini consent.
void ResetGeminiConsent(PrefService* prefs);
FREState CurrentFREState(PrefService* prefs);
bool DidUserConsentToGemini(PrefService* prefs);
bool DidUserSeeGeminiPromo(PrefService* prefs);
bool DidGeminiLiveIntroPlay(PrefService* prefs);
void UpdateUserConsentPrefs(bool consent, PrefService* prefs);
}  // namespace gemini

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_PREFS_H_
