// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/dolby_vision_metadata.h"

namespace media {
namespace {
// Remove inserted HEVC emulation prevention bytes from the payload
std::vector<uint8_t> RemoveEmulationBytes(base::span<const uint8_t> input) {
  std::vector<uint8_t> output;
  output.reserve(input.size());

  size_t i = 0;
  while (i < input.size()) {
    // In H.26x byte streams, 0x03 is only an inserted emulation-prevention
    // byte when followed by 0x00..0x03.
    if (i + 3 < input.size() && input[i] == 0x00 && input[i + 1] == 0x00 &&
        input[i + 2] == 0x03 && input[i + 3] <= 0x03) {
      output.push_back(0x00);
      output.push_back(0x00);
      i += 3;
    } else {
      output.push_back(input[i++]);
    }
  }

  return output;
}
}  // namespace

DolbyVisionMetadata::DolbyVisionMetadata() = default;
DolbyVisionMetadata::DolbyVisionMetadata(const DolbyVisionMetadata&) = default;
DolbyVisionMetadata::DolbyVisionMetadata(DolbyVisionMetadata&&) = default;
DolbyVisionMetadata& DolbyVisionMetadata::operator=(
    const DolbyVisionMetadata&) = default;
DolbyVisionMetadata& DolbyVisionMetadata::operator=(DolbyVisionMetadata&&) =
    default;
DolbyVisionMetadata::~DolbyVisionMetadata() = default;

DolbyVisionMetadata DolbyVisionMetadata::FromRaw(base::span<const uint8_t> data,
                                                 base::TimeDelta timestamp) {
  DolbyVisionMetadata metadata;
  metadata.data = std::vector<uint8_t>(data.begin(), data.end());
  metadata.timestamp = timestamp;
  return metadata;
}

DolbyVisionMetadata DolbyVisionMetadata::FromH265(
    base::span<const uint8_t> data,
    base::TimeDelta timestamp) {
  DolbyVisionMetadata metadata;
  metadata.data = RemoveEmulationBytes(data);
  metadata.timestamp = timestamp;
  return metadata;
}

}  // namespace media
