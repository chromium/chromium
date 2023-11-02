// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/dts.h"

#include "base/logging.h"
#include "media/base/bit_reader.h"
#include "media/formats/mp4/rcheck.h"

namespace media {

namespace mp4 {

DTS::DTS() = default;

DTS::DTS(const DTS& other) = default;

DTS::~DTS() = default;

bool DTS::Parse(const std::vector<uint8_t>& data, MediaLog* media_log) {
  if (data.empty())
    return false;

  if (data.size() < (32 * 3 + 8 + 2 + 8) / 8)
    return false;

  // Parse ddts box using reader.
  BitReader reader(&data[0], data.size());

  // Parse Sample frequency
  RCHECK(reader.ReadBits(32, &dts_sampling_frequency_));

  // Parse Max Bitrate
  RCHECK(reader.ReadBits(32, &max_bitrate_));

  // Parse Avg Bitrate
  RCHECK(reader.ReadBits(32, &avg_bitrate_));

  // Parse PCM Sample Depth
  RCHECK(reader.ReadBits(8, &pcm_sample_depth_));

  // Parse Frame Duration
  uint8_t frame_duration_code = 0;
  RCHECK(reader.ReadBits(2, &frame_duration_code));
  switch (frame_duration_code) {
    case 0:
      frame_duration_ = 512;
      break;
    case 1:
      frame_duration_ = 1024;
      break;
    case 2:
      frame_duration_ = 2048;
      break;
    case 3:
      frame_duration_ = 4096;
      break;
    default:
      frame_duration_ = 0;
      break;
  }

  LogDtsParameters();

  return true;
}

int DTS::GetFrameDuration() const {
  return frame_duration_;
}

uint32_t DTS::GetDtsSamplingFrequency() const {
  return dts_sampling_frequency_;
}

uint32_t DTS::GetMaxBitrate() const {
  return max_bitrate_;
}

uint32_t DTS::GetAvgBitrate() const {
  return avg_bitrate_;
}

uint8_t DTS::GetPcmSampleDepth() const {
  return pcm_sample_depth_;
}

void DTS::LogDtsParameters() {
  DVLOG(3) << "dts_sampling_freq " << dts_sampling_frequency_ << "max_bitrate "
           << max_bitrate_ << "avg_bitrate " << avg_bitrate_
           << "pcm_sample_depth " << static_cast<int>(pcm_sample_depth_)
           << "frame_duration " << frame_duration_;
}

}  // namespace mp4
}  // namespace media
