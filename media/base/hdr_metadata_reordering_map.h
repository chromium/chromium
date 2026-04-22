// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_HDR_METADATA_REORDERING_MAP_H_
#define MEDIA_BASE_HDR_METADATA_REORDERING_MAP_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

class DecoderBuffer;

// A helper class to manage per-frame HDR metadata indexed by timestamp.
// This is useful for decoders that receive metadata in decode order but
// produce frames in presentation order.
class MEDIA_EXPORT HdrMetadataReorderingMap {
 public:
  HdrMetadataReorderingMap();
  HdrMetadataReorderingMap(const HdrMetadataReorderingMap&) = delete;
  HdrMetadataReorderingMap& operator=(const HdrMetadataReorderingMap&) = delete;
  ~HdrMetadataReorderingMap();

  // If `buffer` has HDR metadata, then insert it into the map.
  void Insert(const DecoderBuffer& buffer);

  // Looks up the HDR metadata for a given timestamp. If found, it is merged
  // into `metadata`. In all cases, all entries with a timestamp less than or
  // equal to `timestamp` are erased (because all users of this produce frames
  // in presentation timestamp order).
  void MergeAndEraseMetadataForTimestamp(base::TimeDelta timestamp,
                                         gfx::HDRMetadata& metadata);

  // Clears all stored metadata.
  void Clear();

 private:
  base::flat_map<base::TimeDelta, gfx::HDRMetadata>
      hdr_metadata_reordering_map_;
};

}  // namespace media

#endif  // MEDIA_BASE_HDR_METADATA_REORDERING_MAP_H_
