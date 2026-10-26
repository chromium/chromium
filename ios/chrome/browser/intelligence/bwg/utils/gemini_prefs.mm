// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"

#import "base/time/time.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace gemini {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kBwgSessionMap);
  registry->RegisterBooleanPref(
      prefs::kIOSBwgConsent, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kIOSGeminiLiveConsent, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kIOSGeminiLiveIntroPlayed, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kIOSBWGPreciseLocationSetting, false);
  registry->RegisterBooleanPref(prefs::kIOSBWGPageContentSetting, true);
  registry->RegisterIntegerPref(prefs::kIOSBWGPromoImpressionCount, 0);
  registry->RegisterBooleanPref(prefs::kIOSGeminiCameraSetting, false);
  registry->RegisterBooleanPref(prefs::kIOSGeminiSuggestionsSetting, true);
  registry->RegisterDictionaryPref(prefs::kIOSContextualCueingVerticalUiState);
  registry->RegisterBooleanPref(prefs::kIOSGeminiLiveClosedCaptioningSetting,
                                false);
  registry->RegisterBooleanPref(prefs::kIOSGeminiLiveMicrophoneSetting, true);
  registry->RegisterTimePref(prefs::kLastGeminiInteractionTimestamp,
                             base::Time());
  registry->RegisterTimePref(prefs::kLastGeminiContextualChipDisplayedTimestamp,
                             base::Time());
  registry->RegisterStringPref(prefs::kLastGeminiInteractionURL, std::string());
  registry->RegisterStringPref(prefs::kGeminiConversationId, std::string());
  registry->RegisterBooleanPref(prefs::kAppStoreGeminiPromoTriggered, false);
  registry->RegisterIntegerPref(prefs::kGeminiEnabledByPolicy, 0);
  registry->RegisterIntegerPref(prefs::kGenAiEnabledByPolicy, 0);
  registry->RegisterIntegerPref(prefs::kGeminiActOnWebSettings, 0);
  registry->RegisterListPref(prefs::kGeminiActOnWebAllowedForURLs);
  registry->RegisterListPref(prefs::kGeminiActOnWebBlockedForURLs);
  registry->RegisterBooleanPref(prefs::kAIHubEligibilityTriggered, false);
}

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

FirstRunState CurrentFirstRunState(const PrefService* prefs) {
  // Check consent first as converged FRE flows skip the separate promo step.
  if (DidUserConsentToGemini(prefs)) {
    return FirstRunState::kCompleted;
  }
  if (DidUserSeeGeminiPromo(prefs)) {
    return FirstRunState::kStarted;
  }
  return FirstRunState::kPending;
}

bool DidUserConsentToGemini(const PrefService* prefs) {
  return prefs->GetBoolean(::prefs::kIOSBwgConsent);
}

bool DidUserConsentToGeminiLive(const PrefService* prefs) {
  return prefs->GetBoolean(::prefs::kIOSGeminiLiveConsent);
}

bool DidUserSeeGeminiPromo(const PrefService* prefs) {
  return prefs->GetInteger(prefs::kIOSBWGPromoImpressionCount) > 0;
}

bool DidGeminiLiveIntroPlay(const PrefService* prefs) {
  return prefs->GetBoolean(::prefs::kIOSGeminiLiveIntroPlayed);
}

void SetGeminiLiveIntroPlayed(PrefService* prefs) {
  prefs->SetBoolean(::prefs::kIOSGeminiLiveIntroPlayed, true);
}

void UpdateUserConsentPrefs(bool consent, PrefService* prefs) {
  prefs->SetBoolean(::prefs::kIOSBwgConsent, consent);
}

void UpdateUserConsentToLivePrefs(bool consent, PrefService* prefs) {
  prefs->SetBoolean(::prefs::kIOSGeminiLiveConsent, consent);
}

void CreateOrUpdateConversationIdPrefs(const std::string& conversation_id,
                                       const std::string& url_spec,
                                       PrefService* prefs) {
  if (conversation_id.empty()) {
    return;
  }
  prefs->SetTime(prefs::kLastGeminiInteractionTimestamp, base::Time::Now());
  prefs->SetString(prefs::kLastGeminiInteractionURL, url_spec);
  prefs->SetString(prefs::kGeminiConversationId, conversation_id);
}

std::optional<std::string> GetConversationId(PrefService* prefs) {
  base::Time last_interaction_timestamp =
      prefs->GetTime(prefs::kLastGeminiInteractionTimestamp);
  const std::string conversation_id =
      prefs->GetString(prefs::kGeminiConversationId);
  if (base::Time::Now() - last_interaction_timestamp <
      GetGeminiSessionValidityDuration()) {
    if (!conversation_id.empty()) {
      return conversation_id;
    }
  }
  return std::nullopt;
}

void DeleteGeminiSessionInStorage(PrefService* prefs) {
  prefs->ClearPref(prefs::kGeminiConversationId);
}

}  // namespace gemini
