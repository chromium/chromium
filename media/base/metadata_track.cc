// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/metadata_track.h"

#include <algorithm>

#include "media/base/decoder_buffer.h"
#include "media/base/decoder_buffer_side_data.h"

namespace media {

MetadataTrack::MetadataTrack(IT35PrefixType prefix_type)
    : it35_prefix_type_(prefix_type) {}

MetadataTrack::MetadataTrack(MetadataTrack&&) = default;
MetadataTrack& MetadataTrack::operator=(MetadataTrack&&) = default;
MetadataTrack::~MetadataTrack() = default;

void MetadataTrack::InsertMetadataBuffer(const DecoderBuffer& buffer) {
  gfx::HDRMetadata buffer_metadata;
  switch (it35_prefix_type_) {
    case IT35PrefixType::kSmpteSt2094App5:
      buffer_metadata.SetSerializedAgtm(base::span(buffer));
      break;
    case IT35PrefixType::kUnknown:
      break;
  }
  metadata_.SetInterval(buffer.timestamp(),
                        buffer.timestamp() + buffer.duration(),
                        buffer_metadata);
}

bool MetadataTrack::TryAttachMetadata(DecoderBuffer& buffer) const {
  // Note that IntervalMap::find always returns a value.
  auto metadata = metadata_.find(buffer.timestamp()).value();
  if (!metadata.has_value()) {
    return false;
  }
  buffer.WritableSideData().hdr_metadata.MergeMetadataFrom(*metadata);

  return true;
}

void MetadataTrack::Reset() {
  metadata_.clear();
}

}  // namespace media
