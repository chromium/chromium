// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_MODEL_SPEECH_INPUT_LOCALE_H_
#define IOS_CHROME_BROWSER_VOICE_MODEL_SPEECH_INPUT_LOCALE_H_

#include <string>

namespace voice {

// Struct describing a valid speech input locale.
typedef struct {
  // The locale code in canonical form (e.g. "en-US", "fr-FR").
  std::string code;
  // The display name (e.g. "English U.S.", "Français (France)").
  std::u16string display_name;
} SpeechInputLocale;

}  // namespace voice

#endif  // IOS_CHROME_BROWSER_VOICE_MODEL_SPEECH_INPUT_LOCALE_H_
