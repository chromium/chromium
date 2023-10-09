// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/voice/model/voice_search_prefs.h"

namespace prefs {

// User preferred speech input language for voice search.
const char kVoiceSearchLocale[] = "ios.speechinput.voicesearch_locale";

// Boolean which indicates if TTS after voice search is enabled.
const char kVoiceSearchTTS[] = "ios.speechinput.voicesearch_tts";

}  // namespace prefs
