// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_BITRATE_ALLOCATION_H_
#define MEDIA_BASE_VIDEO_BITRATE_ALLOCATION_H_

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "media/base/media_export.h"

namespace media {

// Class that describes how video bitrate, in bps, is allocated across temporal
// and spatial layers. Note that bitrates are NOT cumulative. Depending on if
// layers are dependent or not, it is up to the user to aggregate.
class MEDIA_EXPORT VideoBitrateAllocation {
 public:
  static constexpr size_t kMaxSpatialLayers = 5;
  static constexpr size_t kMaxTemporalLayers = 4;

  VideoBitrateAllocation();
  ~VideoBitrateAllocation() = default;

  // Returns if this bitrate can't be set (sum exceeds int max value).
  bool SetBitrate(size_t spatial_index, size_t temporal_index, int bitrate_bps);

  // Returns the bitrate for specified spatial/temporal index, or 0 if not set.
  int GetBitrateBps(size_t spatial_index, size_t temporal_index) const;

  // Sum of all bitrates.
  int32_t GetSumBps() const;

  std::string ToString() const;

  bool operator==(const VideoBitrateAllocation& other) const;
  inline bool operator!=(const VideoBitrateAllocation& other) const {
    return !(*this == other);
  }

 private:
  int sum_;  // Cached sum of all elements of |bitrates_|, for performance.
  int bitrates_[kMaxSpatialLayers][kMaxTemporalLayers];
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_BITRATE_ALLOCATION_H_
