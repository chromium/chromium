// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "video_bitrate_allocation.h"

#include <cstring>
#include <limits>
#include <numeric>
#include <sstream>

#include "base/check_op.h"
#include "base/numerics/checked_math.h"

namespace media {

constexpr size_t VideoBitrateAllocation::kMaxSpatialLayers;
constexpr size_t VideoBitrateAllocation::kMaxTemporalLayers;

VideoBitrateAllocation::VideoBitrateAllocation() : sum_(0), bitrates_{} {}

bool VideoBitrateAllocation::SetBitrate(size_t spatial_index,
                                        size_t temporal_index,
                                        int bitrate_bps) {
  CHECK_LT(spatial_index, kMaxSpatialLayers);
  CHECK_LT(temporal_index, kMaxTemporalLayers);
  CHECK_GE(bitrate_bps, 0);

  base::CheckedNumeric<int> checked_sum = sum_;
  checked_sum -= bitrates_[spatial_index][temporal_index];
  checked_sum += bitrate_bps;
  if (!checked_sum.IsValid()) {
    return false;  // Would cause overflow of the sum.
  }

  sum_ = checked_sum.ValueOrDie();
  bitrates_[spatial_index][temporal_index] = bitrate_bps;
  return true;
}

int VideoBitrateAllocation::GetBitrateBps(size_t spatial_index,
                                          size_t temporal_index) const {
  CHECK_LT(spatial_index, kMaxSpatialLayers);
  CHECK_LT(temporal_index, kMaxTemporalLayers);
  return bitrates_[spatial_index][temporal_index];
}

int VideoBitrateAllocation::GetSumBps() const {
  return sum_;
}

std::string VideoBitrateAllocation::ToString() const {
  size_t num_active_spatial_layers = 0;
  size_t num_temporal_layers[kMaxSpatialLayers] = {};
  for (size_t sid = 0; sid < kMaxSpatialLayers; ++sid) {
    for (size_t tid = 0; tid < kMaxTemporalLayers; ++tid) {
      if (bitrates_[sid][tid] > 0)
        num_temporal_layers[sid] = tid + 1;
    }
    if (num_temporal_layers[sid] > 0)
      num_active_spatial_layers += 1;
  }

  if (num_active_spatial_layers == 0) {
    // VideoBitrateAllocation containing no positive value is used to pause an
    // encoder in webrtc.
    return "Empty VideoBitrateAllocation";
  }

  std::stringstream ss;
  ss << "active spatial layers: " << num_active_spatial_layers;
  ss << ", {";

  bool first_sid = true;
  for (size_t sid = 0; sid < kMaxSpatialLayers; ++sid) {
    if (num_temporal_layers[sid] == 0)
      continue;
    if (!first_sid)
      ss << ", ";
    first_sid = false;
    ss << "SL#" << sid << ": {";
    for (size_t tid = 0; tid < num_temporal_layers[sid]; ++tid) {
      if (tid)
        ss << ", ";
      ss << bitrates_[sid][tid];
    }
    ss << "}";
  }
  ss << "}";
  return ss.str();
}

bool VideoBitrateAllocation::operator==(
    const VideoBitrateAllocation& other) const {
  if (sum_ != other.sum_) {
    return false;
  }
  return memcmp(bitrates_, other.bitrates_, sizeof(bitrates_)) == 0;
}

}  // namespace media
