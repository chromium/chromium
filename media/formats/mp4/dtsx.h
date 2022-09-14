// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_DTSX_H_
#define MEDIA_FORMATS_MP4_DTSX_H_

#include <stdint.h>

#include <vector>

#include "media/base/media_export.h"
#include "media/base/media_log.h"

namespace media {

namespace mp4 {

enum class DtsxChannelMask : uint32_t {
  C = 0x00000001,
  L = 0x00000002,
  R = 0x00000004,
  LS = 0x00000008,
  RS = 0x00000010,
  LFE1 = 0x00000020,
  CS = 0x00000040,
  LSR = 0x00000080,
  RSR = 0x00000100,
  LSS = 0x00000200,
  RSS = 0x00000400,
  LC = 0x00000800,
  RC = 0x00001000,
  LH = 0x00002000,
  CH = 0x00004000,
  RH = 0x00008000,
  LFE2 = 0x00010000,
  LW = 0x00020000,
  RW = 0x00040000,
  OH = 0x00080000,
  LHS = 0x00100000,
  RHS = 0x00200000,
  CHR = 0x00400000,
  LHR = 0x00800000,
  RHR = 0x01000000,
  CB = 0x02000000,
  LB = 0x04000000,
  RB = 0x08000000,
  LTF = 0x10000000,
  RTF = 0x20000000,
  LTR = 0x40000000,
  RTR = 0x80000000
};

// This class parses the DTSX information from decoder specific information
// embedded in the udts box in an ISO BMFF file.
// Please refer to SCTE DVS 243-4 Part 4 Table 12 - DTS-UHD Specific Box for
// more details.
class MEDIA_EXPORT DTSX {
 public:
  DTSX();
  DTSX(const DTSX& other);
  ~DTSX();

  // Parse the DTSX config from the udts box.
  bool Parse(const std::vector<uint8_t>& data, MediaLog* media_log);

  uint8_t GetDecoderProfileCode() const;
  int GetFrameDuration() const;
  int GetMaxPayload() const;
  int GetNumPresentations() const;
  uint32_t GetChannelMask() const;
  int GetSamplingFrequency() const;

 private:
  // Logs the parameters of a DTSX stream to DVLOG level 3.
  void LogDtsxParameters();

  // Indicates the DTS-UHD decoder profile required to decode this stream
  uint8_t decoder_profile_code_ = 0;

  // Frame duration in samples.
  // Relative to BaseSamplingFrequency.
  int frame_duration_ = 0;

  // Indicates the maximum size of the audio payload.
  // Maxpayload is not the size of the largest audio frame in the presentation,
  // but rather a "not to exceed" value for buffer configuration and digital
  // audio interface purposes, and is inclusive of all required preambles,
  // headers, burst spacing, etc.
  int max_payload_ = 0;

  // The num of audio presentations encoded within DTS-UHD elementary stream.
  int num_presentations_ = 0;

  // A bit mask that indicates the channel layout encoded in the default
  // resentation
  // of the DTS-UHD bitstream.
  uint32_t channel_mask_ = 0;

  // The sampling frequency of the audio samples stored in the bitstream.
  // Calucalated by multiplying the BaseSamplingFrequency by SampleRateMod.
  int sampling_frequency_ = 0;
};

}  // namespace mp4

}  // namespace media

#endif  // MEDIA_FORMATS_MP4_DTSX_H_
