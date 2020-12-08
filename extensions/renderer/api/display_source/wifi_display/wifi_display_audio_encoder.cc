// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_audio_encoder.h"

#include "base/check_op.h"
#include "base/notreached.h"

namespace extensions {

WiFiDisplayAudioEncoder::WiFiDisplayAudioEncoder(
    const wds::AudioCodec& audio_codec)
    : audio_codec_(audio_codec) {}

WiFiDisplayAudioEncoder::~WiFiDisplayAudioEncoder() = default;

void WiFiDisplayAudioEncoder::Create(const wds::AudioCodec& audio_codec,
                                     AudioEncoderCallback encoder_callback) {
  // Create a format specific encoder.
  switch (audio_codec.format) {
    case wds::LPCM:
      CreateLPCM(audio_codec, std::move(encoder_callback));
      return;
    default:
      break;
  }

  // Report failure.
  std::move(encoder_callback).Run(nullptr);
}

size_t WiFiDisplayAudioEncoder::GetAudioCodecMode() const {
  DCHECK_EQ(1u, audio_codec_.modes.count());
  for (size_t mode = 0u; mode < audio_codec_.modes.size(); ++mode) {
    if (audio_codec_.modes.test(mode))
      return mode;
  }
  NOTREACHED();
  return kInvalidCodecModeValue;
}

}  // namespace extensions
