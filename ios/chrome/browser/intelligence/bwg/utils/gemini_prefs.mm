// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace gemini {
bool GeminiAllowedByPolicy(PrefService* prefs) {
  return GenAiAllowedByEnterprise(prefs) && GeminiAllowedByEnterprise(prefs);
}

bool GenAiAllowedByEnterprise(PrefService* prefs) {
  return prefs->GetInteger(::prefs::kGenAiEnabledByPolicy) !=
         static_cast<int>(gemini::GenAiDefaultSettingsPolicy::kNotAllowed);
}

bool GeminiAllowedByEnterprise(PrefService* prefs) {
  return prefs->GetInteger(::prefs::kGeminiEnabledByPolicy) !=
         static_cast<int>(gemini::SettingsPolicy::kNotAllowed);
}

void ResetGeminiConsent(PrefService* prefs) {
  UpdateUserConsentPrefs(false, prefs);
}

FREState CurrentFREState(PrefService* prefs) {
  if (DidUserSeeGeminiPromo(prefs)) {
    return DidUserConsentToGemini(prefs) ? FREState::kCompleted
                                         : FREState::kStarted;
  }

  return FREState::kPending;
}

bool DidUserConsentToGemini(PrefService* prefs) {
  return prefs->GetBoolean(::prefs::kIOSBwgConsent);
}

bool DidUserSeeGeminiPromo(PrefService* prefs) {
  return prefs->GetInteger(prefs::kIOSBWGPromoImpressionCount) > 0;
}

bool DidGeminiLiveIntroPlay(PrefService* prefs) {
  return prefs->GetBoolean(::prefs::kIOSGeminiLiveIntroPlayed);
}

void UpdateUserConsentPrefs(bool value, PrefService* prefs) {
  prefs->SetBoolean(::prefs::kIOSBwgConsent, value);
}
}  // namespace gemini
