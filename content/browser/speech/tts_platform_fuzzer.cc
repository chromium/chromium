// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "build/build_config.h"
#include "content/public/browser/tts_platform.h"

#if defined(OS_WIN)
#include <objbase.h>
#endif

namespace content {

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  int utterance_id = 0;
  std::string utterance;
  std::string lang;
  VoiceData voice;
  UtteranceContinuousParameters params;
  params.pitch = 1.0;
  params.rate = 1.0;
  params.volume = 0.1;

#if defined(OS_WIN)
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  }
#endif

  // First byte gives us the utterance ID.
  size_t i = 0;
  if (i < size)
    utterance_id = data[i++];

  // Decide whether we want to fuzz the language, rate, pitch,
  // voice name, and all that. Half the time we'll just leave
  // those empty and fuzz only the utterance, otherwise it's
  // possible the fuzzer would never spend any effort fuzzing
  // utteranes.
  if (i < size && (data[i++] % 2 == 0)) {
    // The next few bytes determine the language. Ensure that
    // we frequently get some common real languages but allow
    // arbitrary strings up to 10 characters.
    if (i < size) {
      int lang_choice = data[i++];
      switch (lang_choice) {
        case 0:
          lang = "";
          break;
        case 1:
          lang = "en";
          break;
        case 2:
          lang = "fr";
          break;
        case 3:
          lang = "es";
          break;
        default:
          int lang_len = 1 + (lang_choice - 4) % 10;
          for (int j = 0; j < lang_len; j++) {
            if (i < size)
              lang.append(1, data[i++]);
          }
      }
    }

    // Set native_voice_identifier
    if (i < size) {
      int voice_len = data[i++] % 10;
      for (int j = 0; j < voice_len; j++) {
        if (i < size)
          voice.native_voice_identifier.append(1, data[i++]);
      }
    }

    // Set rate, pitch.
    if (i + 4 <= size) {
      params.rate = *reinterpret_cast<const float*>(&data[i]);
      i += 4;
    }
    if (i + 4 <= size) {
      params.pitch = *reinterpret_cast<const float*>(&data[i]);
      i += 4;
    }
  }

  // The rest of the data becomes the utterance.
  while (i < size)
    utterance.append(1, data[i++]);

  TtsPlatform* tts = TtsPlatform::GetInstance();
  CHECK(tts->PlatformImplAvailable());

  VLOG(1) << "id=" << utterance_id << " lang='" << lang << "'"
          << " voice='" << voice.native_voice_identifier << "'"
          << " pitch=" << params.pitch << " rate=" << params.rate
          << " volume=" << params.volume << " utterance='" << utterance << "'";

  tts->StopSpeaking();
  tts->Speak(utterance_id, utterance, lang, voice, params,
             base::BindOnce([](bool success) {}));

  return 0;
}

}  // namespace content
