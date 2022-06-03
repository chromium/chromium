// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "media/audio/wav_audio_handler.h"
#include "media/base/audio_bus.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOG_FATAL); }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  base::StringPiece wav_data(reinterpret_cast<const char*>(data), size);
  std::unique_ptr<media::WavAudioHandler> handler =
      media::WavAudioHandler::Create(wav_data);

  // Abort early to avoid crashing inside AudioBus's ValidateConfig() function.
  if (!handler || handler->total_frames() <= 0) {
    return 0;
  }

  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(handler->num_channels(), handler->total_frames());
  size_t bytes_written;
  handler->CopyTo(audio_bus.get(), 0, &bytes_written);
  return 0;
}
