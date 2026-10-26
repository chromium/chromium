// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_PREFS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_PREFS_H_

#import <optional>
#import <string>

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace gemini {

// Registers the profile prefs associated with Gemini.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Enterprise policies allow for Gemini usage within Chrome.
bool GeminiAllowedByPolicy(PrefService* prefs);
bool GenAiAllowedByEnterprise(PrefService* prefs);
bool GeminiAllowedByEnterprise(PrefService* prefs);

// Functions that interact with the Gemini consent.
void ResetGeminiConsent(PrefService* prefs);
// Returns the current `FirstRunState` for Gemini based on user prefs.
FirstRunState CurrentFirstRunState(const PrefService* prefs);
bool DidUserConsentToGemini(const PrefService* prefs);
bool DidUserConsentToGeminiLive(const PrefService* prefs);
bool DidUserSeeGeminiPromo(const PrefService* prefs);
bool DidGeminiLiveIntroPlay(const PrefService* prefs);
void SetGeminiLiveIntroPlayed(PrefService* prefs);
void UpdateUserConsentPrefs(bool consent, PrefService* prefs);

// Updates the user consent preference for Gemini Live. When consenting
// (`consent` is true), also approves the Live microphone setting.
void UpdateUserConsentToLivePrefs(bool consent, PrefService* prefs);

// Creates a new Gemini session in the prefs, or updates an existing one, with
// the current timestamp.
void CreateOrUpdateConversationIdPrefs(const std::string& conversation_id,
                                       const std::string& url_spec,
                                       PrefService* prefs);

// Retrieves the stored conversation ID from storage if the session is still
// valid.
std::optional<std::string> GetConversationId(PrefService* prefs);

// Removes the associated WebState's session from storage.
void DeleteGeminiSessionInStorage(PrefService* prefs);

}  // namespace gemini

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_PREFS_H_
