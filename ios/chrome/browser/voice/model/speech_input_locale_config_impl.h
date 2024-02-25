// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_MODEL_SPEECH_INPUT_LOCALE_CONFIG_IMPL_H_
#define IOS_CHROME_BROWSER_VOICE_MODEL_SPEECH_INPUT_LOCALE_CONFIG_IMPL_H_

#import <Foundation/Foundation.h>

#include <map>
#include <string>
#include <vector>

#include "ios/chrome/browser/voice/model/speech_input_locale_config.h"
#include "ios/chrome/browser/voice/model/speech_input_locale_match.h"
#include "ios/chrome/browser/voice/model/voice_search_language.h"

namespace voice {

// A concrete implementation of SpeechInputLocaleConfig that uses the available
// language list from S3Kit and the speech input locale matching from
// SpeechInputLocaleMatches.plist.
class SpeechInputLocaleConfigImpl : public SpeechInputLocaleConfig {
 public:
  SpeechInputLocaleConfigImpl(NSArray<VoiceSearchLanguage*>* languages,
                              NSArray<SpeechInputLocaleMatch*>* locale_matches);

  SpeechInputLocaleConfigImpl(const SpeechInputLocaleConfigImpl&) = delete;
  SpeechInputLocaleConfigImpl& operator=(const SpeechInputLocaleConfigImpl&) =
      delete;

  ~SpeechInputLocaleConfigImpl() override;

  // Returns the available locale that matches `locale_code`.  Defaults to en-US
  // if a matching locale is not found.
  SpeechInputLocale GetMatchingLocale(const std::string& locale_code) const;

  // SpeechInputLocaleConfig:
  SpeechInputLocale GetDefaultLocale() const override;
  const std::vector<SpeechInputLocale>& GetAvailableLocales() const override;
  SpeechInputLocale GetLocaleForCode(
      const std::string& locale_code) const override;
  const std::vector<std::string>& GetTextToSpeechLanguages() const override;
  bool IsTextToSpeechEnabledForCode(
      const std::string& locale_code) const override;

 private:
  // Returns a canonical locale code created from combining the UI language
  // preference from NSLocale's |+preferredLanguages` and the country code from
  // the device's locale.
  std::string GetDefaultLocaleCode() const;

  // Populates `available_locales_` using S3Kit's language manager.
  void InitializeAvailableLocales(NSArray<VoiceSearchLanguage*>* languages);

  // Adds local matching data from speech_input_matches.plist into
  // `locale_indices_for_codes_`.
  void InitializeLocaleMatches(
      NSArray<SpeechInputLocaleMatch*>* locale_matches);

  // Populates `text_to_speech_languages_` with the available locales.
  void InitializeTextToSpeechLanguages();

  // The list of available speech input locales.
  std::vector<SpeechInputLocale> available_locales_;
  // A map storing canonical locale codes with the index of their associated
  // InputLocale within `available_locales_`.
  std::map<std::string, size_t> locale_indices_for_codes_;
  // A map storing the language portions of locale codes with the index of their
  // associated InputLocale within `available_locales_`.
  std::map<std::string, size_t> default_locale_indices_for_languages_;
  // The languages available for Text To Speech search results.
  std::vector<std::string> text_to_speech_languages_;
};

}  // namespace voice

#endif  // IOS_CHROME_BROWSER_VOICE_MODEL_SPEECH_INPUT_LOCALE_CONFIG_IMPL_H_
