// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer_side_data.h"

namespace media {

DecoderBufferSideData::DecoderBufferSideData() = default;

DecoderBufferSideData::~DecoderBufferSideData() = default;

DecoderBufferSideData::DecoderBufferSideData(
    const DecoderBufferSideData& other) = default;

bool DecoderBufferSideData::Matches(const DecoderBufferSideData& other) const {
  return spatial_layers == other.spatial_layers &&
         alpha_data == other.alpha_data && secure_handle == other.secure_handle;
}

}  // namespace media
