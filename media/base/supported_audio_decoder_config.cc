// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/supported_audio_decoder_config.h"

namespace media {

SupportedAudioDecoderConfig::SupportedAudioDecoderConfig() = default;

SupportedAudioDecoderConfig::SupportedAudioDecoderConfig(
    AudioCodec codec,
    AudioCodecProfile profile)
    : codec(codec), profile(profile) {}

SupportedAudioDecoderConfig::~SupportedAudioDecoderConfig() = default;

}  // namespace media
