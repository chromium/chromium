// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_MODEL_VOICE_SEARCH_PREFS_H_
#define IOS_CHROME_BROWSER_VOICE_MODEL_VOICE_SEARCH_PREFS_H_

namespace prefs {

// User preferred speech input language for voice search.
inline constexpr char kVoiceSearchLocale[] =
    "ios.speechinput.voicesearch_locale";

// Boolean which indicates if TTS after voice search is enabled.
inline constexpr char kVoiceSearchTTS[] = "ios.speechinput.voicesearch_tts";

}  // namespace prefs

#endif  // IOS_CHROME_BROWSER_VOICE_MODEL_VOICE_SEARCH_PREFS_H_
