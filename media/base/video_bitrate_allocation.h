// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_BITRATE_ALLOCATION_H_
#define MEDIA_BASE_VIDEO_BITRATE_ALLOCATION_H_

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "media/base/bitrate.h"
#include "media/base/media_export.h"

namespace media {

// Class that describes how video bitrate, in bps, is allocated across temporal
// and spatial layers. Note that bitrates are NOT cumulative. Depending on if
// layers are dependent or not, it is up to the user to aggregate.
class MEDIA_EXPORT VideoBitrateAllocation {
 public:
  static constexpr size_t kMaxSpatialLayers = 5;
  static constexpr size_t kMaxTemporalLayers = 4;

  explicit VideoBitrateAllocation(
      Bitrate::Mode mode = Bitrate::Mode::kConstant);
  ~VideoBitrateAllocation() = default;

  // Returns true iff. the bitrate was set (sum within uint32_t max value). If
  // a variable bitrate is used and the previous peak bitrate was below the new
  // sum of bitrates across layers, this will automatically set the new peak to
  // equal the new sum. If you have a signed or 64-bit value you want to use as
  // input, you must explicitly convert to uint32_t before calling. This is
  // intended to prevent implicit and unsafe type conversion.
  bool SetBitrate(size_t spatial_index,
                  size_t temporal_index,
                  uint32_t bitrate_bps);

  // Deleted variants: you must SAFELY convert to uint32_t before calling.
  // See base/numerics/safe_conversions.h for functions to safely convert
  // between types.
  bool SetBitrate(size_t spatial_index,
                  size_t temporal_index,
                  int32_t bitrate_bps) = delete;
  bool SetBitrate(size_t spatial_index,
                  size_t temporal_index,
                  int64_t bitrate_bps) = delete;
  bool SetBitrate(size_t spatial_index,
                  size_t temporal_index,
                  uint64_t bitrate_bps) = delete;

  // True iff. this bitrate allocation can have its peak set to |peak_bps| (the
  // peak must be greater than the sum of the layers' bitrates, and the bitrate
  // mode must be variable bitrate).
  bool SetPeakBps(uint32_t peak_bps);

  // Returns the bitrate for specified spatial/temporal index, or 0 if not set.
  uint32_t GetBitrateBps(size_t spatial_index, size_t temporal_index) const;

  // Sum of all bitrates.
  uint32_t GetSumBps() const;

  // Returns peak bitrate.
  uint32_t GetPeakBps() const;
  // Non-layered bitrate allocation. If there are layers, this bitrate's target
  // bps equals the sum of the layers' bitrates.
  const Bitrate GetSumBitrate() const;

  // Returns the encoding rate control mode of this allocation.
  Bitrate::Mode GetMode() const;

  std::string ToString() const;

  bool operator==(const VideoBitrateAllocation& other) const;
  inline bool operator!=(const VideoBitrateAllocation& other) const {
    return !(*this == other);
  }

 private:
  // A bitrate representing a cached sum of the elements of |bitrates_|, for
  // performance.
  Bitrate sum_bitrate_;
  uint32_t bitrates_[kMaxSpatialLayers][kMaxTemporalLayers] = {};
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_BITRATE_ALLOCATION_H_
