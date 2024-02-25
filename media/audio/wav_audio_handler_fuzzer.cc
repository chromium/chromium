// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>

#include "base/logging.h"
#include "media/audio/wav_audio_handler.h"
#include "media/base/audio_bus.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  std::string_view wav_data(reinterpret_cast<const char*>(data), size);
  std::unique_ptr<media::WavAudioHandler> handler =
      media::WavAudioHandler::Create(wav_data);

  // Abort early to avoid crashing inside AudioBus's ValidateConfig() function.
  if (!handler || !handler->Initialize() ||
      handler->total_frames_for_testing() <= 0) {
    return 0;
  }

  std::unique_ptr<media::AudioBus> audio_bus = media::AudioBus::Create(
      handler->GetNumChannels(), handler->total_frames_for_testing());
  size_t frames_written;
  handler->CopyTo(audio_bus.get(), &frames_written);
  return 0;
}
