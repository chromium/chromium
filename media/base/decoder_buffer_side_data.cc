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

DecoderBufferSideData& DecoderBufferSideData::operator=(
    DecoderBufferSideData&&) = default;

DecoderBufferSideData::DecoderBufferSideData(DecoderBufferSideData&& other) =
    default;

bool DecoderBufferSideData::Matches(const DecoderBufferSideData& other) const {
  return spatial_layers == other.spatial_layers &&
         alpha_data.as_span() == other.alpha_data.as_span() &&
         secure_handle == other.secure_handle &&
         discard_padding == other.discard_padding &&
         next_config == other.next_config;
}

std::unique_ptr<DecoderBufferSideData> DecoderBufferSideData::Clone() const {
  auto result = std::make_unique<DecoderBufferSideData>();
  result->spatial_layers = spatial_layers;
  if (!alpha_data.empty()) {
    result->alpha_data = base::HeapArray<uint8_t>::CopiedFrom(alpha_data);
  }
  result->secure_handle = secure_handle;
  result->discard_padding = discard_padding;
  result->next_config = next_config;
  return result;
}

}  // namespace media
