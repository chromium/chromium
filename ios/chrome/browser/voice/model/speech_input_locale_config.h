// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_MODEL_SPEECH_INPUT_LOCALE_CONFIG_H_
#define IOS_CHROME_BROWSER_VOICE_MODEL_SPEECH_INPUT_LOCALE_CONFIG_H_

#include <string>
#include <vector>

#include "ios/chrome/browser/voice/model/speech_input_locale.h"

namespace voice {

class SpeechInputLocaleConfig;

// Configuration object supplying information about valid locales for Voice
// Search.
class SpeechInputLocaleConfig {
 public:
  // Returns a pointer to the singleton object.
  static SpeechInputLocaleConfig* GetInstance();

  SpeechInputLocaleConfig(const SpeechInputLocaleConfig&) = delete;
  SpeechInputLocaleConfig& operator=(const SpeechInputLocaleConfig&) = delete;

  // Returns the default locale as determined by the system language.
  virtual SpeechInputLocale GetDefaultLocale() const = 0;

  // Returns a reference to a vector of SpeechInputLocales sorted alphabetically
  // by their display names.
  virtual const std::vector<SpeechInputLocale>& GetAvailableLocales() const = 0;

  // Returns the SpeechInputLocale to use for `locale_code`. If `locale_code` is
  // not contained in GetAvailableLocales()'s return value, then the
  // SpeechInputLocaleConfig will attempt to match with an appropriate subsitute
  // (e.g. "en-NZ" => "en-AU").
  virtual SpeechInputLocale GetLocaleForCode(
      const std::string& locale_code) const = 0;

  // Returns a reference to an alphabetically sorted vector containing language
  // codes (e.g. "en", "fr") that can be used to trigger Text To Speech results.
  virtual const std::vector<std::string>& GetTextToSpeechLanguages() const = 0;

  // Returns whether the language portion of `locale_code` is an available
  // TTS language.
  virtual bool IsTextToSpeechEnabledForCode(
      const std::string& locale_code) const = 0;

 protected:
  SpeechInputLocaleConfig() = default;
  virtual ~SpeechInputLocaleConfig() = default;
};

}  // namespace voice

#endif  // IOS_CHROME_BROWSER_VOICE_MODEL_SPEECH_INPUT_LOCALE_CONFIG_H_
