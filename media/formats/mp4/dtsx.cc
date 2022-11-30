// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/dtsx.h"

#include "base/logging.h"
#include "media/base/bit_reader.h"
#include "media/formats/mp4/rcheck.h"

namespace media {

namespace mp4 {

DTSX::DTSX() = default;

DTSX::DTSX(const DTSX& other) = default;

DTSX::~DTSX() = default;

bool DTSX::Parse(const std::vector<uint8_t>& data, MediaLog* media_log) {
  if (data.empty())
    return false;

  if (data.size() < (2 + 3 + 5 + 32 + 1 + 2 + 8) / 8)
    return false;
  DVLOG(3) << "dtsx data.size " << data.size();
  // Parse udts box using reader.
  BitReader reader(&data[0], data.size());

  // Read DecoderProfileCode
  RCHECK(reader.ReadBits(6, &decoder_profile_code_));

  // Read FrameDurationCode
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

  // Read MaxPayloadCode
  uint8_t max_payload_code = 0;
  RCHECK(reader.ReadBits(3, &max_payload_code));
  switch (max_payload_code) {
    case 0:
      max_payload_ = 2048;
      break;
    case 1:
      max_payload_ = 4096;
      break;
    case 2:
      max_payload_ = 8192;
      break;
    case 3:
      max_payload_ = 16384;
      break;
    case 4:
      max_payload_ = 32768;
      break;
    case 5:
      max_payload_ = 65536;
      break;
    case 6:
      max_payload_ = 131072;
      break;
    case 7:
    default:
      max_payload_ = 0;
      break;
  }

  // Read NumPresentationsCode
  RCHECK(reader.ReadBits(5, &num_presentations_));

  // Read ChannelMask
  RCHECK(reader.ReadBits(32, &channel_mask_));

  // Read BaseSamplingFrequencyCode
  int base_sampling_frequency = 0;
  uint8_t base_sampling_frequency_code = 0;
  RCHECK(reader.ReadBits(1, &base_sampling_frequency_code));
  if (base_sampling_frequency_code == 1)
    base_sampling_frequency = 48000;
  else
    base_sampling_frequency = 44100;

  // Read SampleRateMod
  int sample_rate_mod = 0;
  uint8_t sample_rate_mod_code;
  RCHECK(reader.ReadBits(2, &sample_rate_mod_code));
  switch (sample_rate_mod) {
    case 0:
      sample_rate_mod = 1;
      break;
    case 1:
      sample_rate_mod = 2;
      break;
    case 2:
      sample_rate_mod = 4;
      break;
    case 3:
      sample_rate_mod = 8;
      break;
    default:
      // error, should not hit default case.
      DLOG(ERROR) << "SampleRateMod invalid value";
      return false;
  }

  // Calculate the Sampling Frequency
  sampling_frequency_ = base_sampling_frequency * sample_rate_mod;

  LogDtsxParameters();
  return true;
}

uint8_t DTSX::GetDecoderProfileCode() const {
  return decoder_profile_code_;
}

int DTSX::GetFrameDuration() const {
  return frame_duration_;
}

int DTSX::GetMaxPayload() const {
  return max_payload_;
}

int DTSX::GetNumPresentations() const {
  return num_presentations_;
}

uint32_t DTSX::GetChannelMask() const {
  return channel_mask_;
}

int DTSX::GetSamplingFrequency() const {
  return sampling_frequency_;
}

void DTSX::LogDtsxParameters() {
  DVLOG(3) << "DecoderProfileCode " << static_cast<int>(decoder_profile_code_)
           << "Frame Duration " << frame_duration_ << "Max Payload "
           << max_payload_ << "Num Presentations " << num_presentations_
           << "Channel Mask " << channel_mask_ << "Sampling Frequency "
           << sampling_frequency_;
}

}  // namespace mp4
}  // namespace media
