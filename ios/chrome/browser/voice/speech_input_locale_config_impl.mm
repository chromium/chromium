// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/voice/speech_input_locale_config_impl.h"

#import <Foundation/Foundation.h>

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/voice/speech_input_locale_match_config.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_language.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the language portion of |locale_code|.
std::string GetLanguageComponentForLocaleCode(const std::string& locale_code) {
  std::vector<std::string> tokens = base::SplitString(
      locale_code, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.empty())
    return std::string();
  return tokens[0];
}

// Use "en-US" as a default value.
const char kEnglishUS[] = "en-US";

// Converts |locale| to its canonical form and returns it as a std::string.
std::string GetCanonicalLocaleForLocale(NSString* locale_code) {
  std::string locale = base::SysNSStringToUTF8(
      [NSLocale canonicalLocaleIdentifierFromString:locale_code]);
  return locale;
}

}  // namespace

namespace voice {

// static
SpeechInputLocaleConfigImpl* SpeechInputLocaleConfigImpl::GetInstance() {
  static base::NoDestructor<SpeechInputLocaleConfigImpl> instance;
  return instance.get();
}

SpeechInputLocaleConfigImpl::SpeechInputLocaleConfigImpl() {
  InitializeAvailableLocales();
  InitializeLocaleMatches();
  InitializeTextToSpeechLangauges();
}

SpeechInputLocaleConfigImpl::~SpeechInputLocaleConfigImpl() {}

SpeechInputLocale SpeechInputLocaleConfigImpl::GetDefaultLocale() const {
  return GetMatchingLocale(GetDefaultLocaleCode());
}

const std::vector<SpeechInputLocale>&
SpeechInputLocaleConfigImpl::GetAvailableLocales() const {
  return available_locales_;
}

SpeechInputLocale SpeechInputLocaleConfigImpl::GetLocaleForCode(
    const std::string& locale_code) const {
  auto index_iterator = locale_indices_for_codes_.find(locale_code);
  if (index_iterator == locale_indices_for_codes_.end())
    return SpeechInputLocale();
  return available_locales_[index_iterator->second];
}

const std::vector<std::string>&
SpeechInputLocaleConfigImpl::GetTextToSpeechLanguages() const {
  return text_to_speech_languages_;
}

bool SpeechInputLocaleConfigImpl::IsTextToSpeechEnabledForCode(
    const std::string& locale_code) const {
  std::string language = GetLanguageComponentForLocaleCode(locale_code);
  return base::Contains(text_to_speech_languages_, language);
}

SpeechInputLocale SpeechInputLocaleConfigImpl::GetMatchingLocale(
    const std::string& locale_code) const {
  // Return exact match if one is found.
  auto index_iterator = locale_indices_for_codes_.find(locale_code);
  if (index_iterator != locale_indices_for_codes_.end())
    return available_locales_[index_iterator->second];
  // If there is no exact match, search for another locale with the same
  // language component.
  std::string language = GetLanguageComponentForLocaleCode(locale_code);
  std::vector<size_t> locale_indices;
  for (auto locale_index_for_code : locale_indices_for_codes_) {
    std::string code = locale_index_for_code.first;
    if (GetLanguageComponentForLocaleCode(code) == language)
      locale_indices.push_back(locale_index_for_code.second);
  }
  // Use en-US as a default value.
  auto it = locale_indices_for_codes_.find(kEnglishUS);
  CHECK(it != locale_indices_for_codes_.end());
  size_t locale_index = it->second;
  if (locale_indices.size() == 1) {
    // If only one match was found, use its associated InputLocale.
    locale_index = locale_indices[0];
  } else if (locale_indices.size() > 1) {
    // If multiple regional differences for the same language are supported
    // (e.g. en-US, en-GB, en-AU, en-NZ), use the default language mapping.
    auto default_locale_iterator =
        default_locale_indices_for_languages_.find(language);
    if (default_locale_iterator != default_locale_indices_for_languages_.end())
      return available_locales_[default_locale_iterator->second];
  }
  return available_locales_[locale_index];
}

std::string SpeechInputLocaleConfigImpl::GetDefaultLocaleCode() const {
  // Default locale code is computed based on the UI language as reported by
  // first object in +preferredLanguages and the device's locale (which
  // corresponds to the "Region Format" in iOS' Settings UI). By combining
  // the two signals, there is a better chance of setting the default
  // Voice Search language to the regionally specific language variant.
  // For example, users who selected English as the UI language and use
  // South Africa Region Format (for date, time, currency, etc.) will
  // default to use en-ZA, English (South Africa), as the Voice Search
  // language.
  NSLocale* current_locale = [NSLocale currentLocale];
  NSLocale* lang_pref_locale = [NSLocale
      localeWithLocaleIdentifier:[[NSLocale preferredLanguages] firstObject]];
  // Prioritize the language portion of |language_pref_locale|.
  NSString* language = [lang_pref_locale objectForKey:NSLocaleLanguageCode];
  if (!language.length)
    language = [current_locale objectForKey:NSLocaleLanguageCode];
  DCHECK(language.length);
  // Prioritize the country portion of |current_locale|.
  NSString* country = [current_locale objectForKey:NSLocaleCountryCode];
  if (!country.length)
    country = [lang_pref_locale objectForKey:NSLocaleCountryCode];
  DCHECK(country.length);
  return GetCanonicalLocaleForLocale(
      [NSString stringWithFormat:@"%@-%@", language, country]);
}

void SpeechInputLocaleConfigImpl::InitializeAvailableLocales() {
  NSArray* languages = ios::GetChromeBrowserProvider()
                           ->GetVoiceSearchProvider()
                           ->GetAvailableLanguages();
  for (VoiceSearchLanguage* language in languages) {
    // Store the InputLocale in |available_locales_|.
    std::string locale_code = GetCanonicalLocaleForLocale(language.identifier);
    DCHECK(locale_code.length());
    voice::SpeechInputLocale locale;
    locale.code = locale_code;
    locale.display_name = base::SysNSStringToUTF16(language.displayName);
    available_locales_.push_back(locale);
    // Store the index of the InputLocale.
    size_t locale_index = available_locales_.size() - 1;
    locale_indices_for_codes_[locale_code] = locale_index;
    // Store a mapping from |language.localizationPreference| to the locale.
    std::string localization_preference =
        GetCanonicalLocaleForLocale(language.localizationPreference);
    if (localization_preference.length())
      locale_indices_for_codes_[localization_preference] = locale_index;
  }
}

void SpeechInputLocaleConfigImpl::InitializeLocaleMatches() {
  NSArray* matches = [SpeechInputLocaleMatchConfig sharedInstance].matches;
  for (SpeechInputLocaleMatch* match in matches) {
    std::string locale = GetCanonicalLocaleForLocale(match.matchedLocaleCode);
    auto index_iterator = locale_indices_for_codes_.find(locale);
    if (index_iterator != locale_indices_for_codes_.end()) {
      size_t index = index_iterator->second;
      for (NSString* matching_locale in match.matchingLocaleCodes) {
        // Record the regional variant matches.
        std::string locale_code = GetCanonicalLocaleForLocale(matching_locale);
        locale_indices_for_codes_[locale_code] = index;
      }
      for (NSString* matching_language in match.matchingLanguages) {
        // Record the default locale for the matching languages.
        std::string language = base::SysNSStringToUTF8(matching_language);
        default_locale_indices_for_languages_[language] = index;
      }
    }
  }
}

void SpeechInputLocaleConfigImpl::InitializeTextToSpeechLangauges() {
  text_to_speech_languages_ = {"de", "en", "es", "fr", "it", "ja", "ko"};
}

}  // namespace voice
