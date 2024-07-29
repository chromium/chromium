// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "video_bitrate_allocation.h"

#include <cstring>
#include <limits>
#include <numeric>
#include <sstream>

#include "base/check_op.h"
#include "base/numerics/checked_math.h"
#include "media/base/bitrate.h"

namespace {

static media::Bitrate MakeReplacementBitrate(const media::Bitrate& old,
                                             uint32_t target_bps,
                                             uint32_t peak_bps) {
  switch (old.mode()) {
    case media::Bitrate::Mode::kConstant:
      return media::Bitrate::ConstantBitrate(target_bps);
    case media::Bitrate::Mode::kVariable:
      return media::Bitrate::VariableBitrate(target_bps, peak_bps);
    case media::Bitrate::Mode::kExternal:
      return media::Bitrate::ExternalRateControl();
  }
}

}  // namespace

namespace media {

constexpr size_t VideoBitrateAllocation::kMaxSpatialLayers;
constexpr size_t VideoBitrateAllocation::kMaxTemporalLayers;

VideoBitrateAllocation::VideoBitrateAllocation(Bitrate::Mode mode) {
  switch (mode) {
    case Bitrate::Mode::kConstant:
      sum_bitrate_ = Bitrate::ConstantBitrate(0u);
      break;
    case Bitrate::Mode::kVariable:
      // For variable bitrates, the peak must not be zero as enforced by
      // Bitrate.
      sum_bitrate_ = Bitrate::VariableBitrate(0u, 1u);
      break;
    case Bitrate::Mode::kExternal:
      // For variable bitrates, the peak must not be zero as enforced by
      // Bitrate.
      sum_bitrate_ = Bitrate::ExternalRateControl();
      break;
  }
}

bool VideoBitrateAllocation::SetPeakBps(uint32_t peak_bps) {
  if (sum_bitrate_.mode() != Bitrate::Mode::kVariable)
    return false;

  if (peak_bps == 0u)
    return false;

  if (sum_bitrate_.target_bps() > peak_bps)
    return false;

  Bitrate old = sum_bitrate_;
  sum_bitrate_ = MakeReplacementBitrate(old, old.target_bps(), peak_bps);
  return true;
}

bool VideoBitrateAllocation::SetBitrate(size_t spatial_index,
                                        size_t temporal_index,
                                        uint32_t bitrate_bps) {
  CHECK_LT(spatial_index, kMaxSpatialLayers);
  CHECK_LT(temporal_index, kMaxTemporalLayers);

  base::CheckedNumeric<uint32_t> checked_sum = sum_bitrate_.target_bps();
  uint32_t old_bitrate_bps = bitrates_[spatial_index][temporal_index];
  checked_sum -= old_bitrate_bps;
  checked_sum += bitrate_bps;
  if (!checked_sum.IsValid()) {
    return false;  // Would cause overflow of the sum.
  }

  const uint32_t new_sum_bps = checked_sum.ValueOrDefault(0u);
  const uint32_t new_peak_bps = std::max(sum_bitrate_.peak_bps(), new_sum_bps);
  sum_bitrate_ =
      MakeReplacementBitrate(sum_bitrate_, new_sum_bps, new_peak_bps);
  bitrates_[spatial_index][temporal_index] = bitrate_bps;
  return true;
}

uint32_t VideoBitrateAllocation::GetBitrateBps(size_t spatial_index,
                                               size_t temporal_index) const {
  CHECK_LT(spatial_index, kMaxSpatialLayers);
  CHECK_LT(temporal_index, kMaxTemporalLayers);
  return bitrates_[spatial_index][temporal_index];
}

uint32_t VideoBitrateAllocation::GetSumBps() const {
  return sum_bitrate_.target_bps();
}

uint32_t VideoBitrateAllocation::GetPeakBps() const {
  return sum_bitrate_.peak_bps();
}

const Bitrate VideoBitrateAllocation::GetSumBitrate() const {
  return sum_bitrate_;
}

Bitrate::Mode VideoBitrateAllocation::GetMode() const {
  return sum_bitrate_.mode();
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
  ss << "}, mode ";
  switch (sum_bitrate_.mode()) {
    case Bitrate::Mode::kConstant:
      ss << "CBR";
      break;
    case Bitrate::Mode::kVariable:
      ss << "VBR with peak bps " << sum_bitrate_.peak_bps();
      break;
    case Bitrate::Mode::kExternal:
      ss << "External rate control";
      break;
  }
  return ss.str();
}

bool VideoBitrateAllocation::operator==(
    const VideoBitrateAllocation& other) const {
  if (sum_bitrate_ != other.sum_bitrate_) {
    return false;
  }
  return memcmp(bitrates_, other.bitrates_, sizeof(bitrates_)) == 0;
}

}  // namespace media
