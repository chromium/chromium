// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/model/speech_input_locale_config.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/voice/model/speech_input_locale_config_impl.h"
#import "ios/chrome/browser/voice/model/speech_input_locale_match.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"

namespace voice {

// static
SpeechInputLocaleConfig* SpeechInputLocaleConfig::GetInstance() {
  static base::NoDestructor<SpeechInputLocaleConfigImpl> instance(
      ios::provider::GetAvailableLanguages(), LoadSpeechInputLocaleMatches());
  return instance.get();
}

}  // namespace voice
