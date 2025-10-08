// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/wav_audio_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "base/logging.h"
#include "media/base/audio_bus.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

extern "C" int LLVMFuzzerTestOneInput(base::span<const uint8_t> data_span) {
  static Environment env;
  std::unique_ptr<media::WavAudioHandler> handler =
      media::WavAudioHandler::Create(data_span);

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
