// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_METADATA_TRACK_H_
#define MEDIA_BASE_METADATA_TRACK_H_

#include <optional>

#include "base/time/time.h"
#include "media/base/interval_map.h"
#include "media/base/media_export.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

class DecoderBuffer;

// Caches metadata from a timed metadata track and attaches it to the buffers of
// a render track that references it.
class MEDIA_EXPORT MetadataTrack {
 public:
  enum class IT35PrefixType {
    kUnknown,
    kSmpteSt2094App5,
  };

  explicit MetadataTrack(IT35PrefixType prefix_type);
  MetadataTrack(const MetadataTrack&) = delete;
  MetadataTrack& operator=(const MetadataTrack&) = delete;
  MetadataTrack(MetadataTrack&&);
  MetadataTrack& operator=(MetadataTrack&&);
  ~MetadataTrack();

  // Parse the metadata in `buffer`, which must be a sample from the timed
  // metadata track, and cache it for the interval
  // [timestamp, timestamp + duration).
  void InsertMetadataBuffer(const DecoderBuffer& buffer);

  // Merge the metadata for `buffer`s timestamp into its side data and return
  // true. Return false if no metadata has been parsed for that timestamp yet,
  // in which case `buffer` is left untouched and the caller must not emit it
  // until a subsequent call returns true.
  [[nodiscard]] bool TryAttachMetadata(DecoderBuffer& buffer) const;

  // Erase all cached metadata.
  void Reset();

 private:
  IT35PrefixType it35_prefix_type_;

  // IntervalMap creates a default interval. In order to distinguish between
  // empty metadata and no metadata, use an optional as the data type.
  IntervalMap<base::TimeDelta, std::optional<gfx::HDRMetadata>> metadata_;
};

}  // namespace media

#endif  // MEDIA_BASE_METADATA_TRACK_H_
