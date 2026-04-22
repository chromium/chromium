// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/hdr_metadata_reordering_map.h"

#include "base/check.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decoder_buffer_side_data.h"

namespace media {

HdrMetadataReorderingMap::HdrMetadataReorderingMap() = default;
HdrMetadataReorderingMap::~HdrMetadataReorderingMap() = default;

void HdrMetadataReorderingMap::Insert(const DecoderBuffer& buffer) {
  if (buffer.end_of_stream() || !buffer.side_data() ||
      buffer.side_data()->hdr_metadata.IsEmpty()) {
    return;
  }
  auto [it, inserted] = hdr_metadata_reordering_map_.insert(
      {buffer.timestamp(), buffer.side_data()->hdr_metadata});
  DCHECK(inserted);
}

void HdrMetadataReorderingMap::MergeAndEraseMetadataForTimestamp(
    base::TimeDelta timestamp,
    gfx::HDRMetadata& metadata) {
  auto it = hdr_metadata_reordering_map_.lower_bound(timestamp);

  if (it != hdr_metadata_reordering_map_.end() && it->first == timestamp) {
    metadata.MergeMetadataFrom(it->second);
    it++;
  }

  hdr_metadata_reordering_map_.erase(hdr_metadata_reordering_map_.begin(), it);
}

void HdrMetadataReorderingMap::Clear() {
  hdr_metadata_reordering_map_.clear();
}

}  // namespace media
