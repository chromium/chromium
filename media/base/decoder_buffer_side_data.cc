// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer_side_data.h"

namespace media {

// Required for equality tests of `next_config`.
bool operator==(const AudioDecoderConfig& config,
                const AudioDecoderConfig& other_config) {
  return config.Matches(other_config);
}
bool operator==(const VideoDecoderConfig& config,
                const VideoDecoderConfig& other_config) {
  return config.Matches(other_config);
}

DecoderBufferSideData::DecoderBufferSideData() = default;

DecoderBufferSideData::~DecoderBufferSideData() = default;

DecoderBufferSideData::DecoderBufferSideData(
    const DecoderBufferSideData& other) = default;

bool DecoderBufferSideData::Matches(const DecoderBufferSideData& other) const {
  return spatial_layers == other.spatial_layers &&
         alpha_data == other.alpha_data &&
         secure_handle == other.secure_handle &&
         discard_padding == other.discard_padding &&
         next_config == other.next_config;
}

}  // namespace media
