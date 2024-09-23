// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_DTS_H_
#define MEDIA_FORMATS_MP4_DTS_H_

#include <stdint.h>

#include <vector>

#include "build/build_config.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/media_buildflags.h"

namespace media {

namespace mp4 {

// This class parses the DTS information from decoder specific information
// embedded in the ddts box in an ISO BMFF file.
// Please refer to ETSI TS 102 114 Annex E.3.3 DTSSpecificBox for more
// details.
class MEDIA_EXPORT DTS {
 public:
  DTS();
  DTS(const DTS& other);
  ~DTS();

  // Parse the DTS config from the ddts box.
  bool Parse(const std::vector<uint8_t>& data, MediaLog* media_log);

  uint32_t GetDtsSamplingFrequency() const;
  uint32_t GetMaxBitrate() const;
  uint32_t GetAvgBitrate() const;
  uint8_t GetPcmSampleDepth() const;
  int GetFrameDuration() const;

 private:
  // Logs the parameters of a DTS stream to DVLOG level 3.
  void LogDtsParameters();

  // The maximum sampling frequency stored in the compressed audio stream.
  uint32_t dts_sampling_frequency_ = 0;

  // The peak bit rate in bits per second.
  // If the stream is a constant bitrate, this shall have the same value as
  // avg_bitrate.
  // If the maximum bitrate is unknown, this shall be set to 0.
  uint32_t max_bitrate_ = 0;

  // The average bitrate in bits per second.
  uint32_t avg_bitrate_ = 0;

  // The bit depth of the rendered audio. For DTS formats this is usually
  //  24-bits.
  uint8_t pcm_sample_depth_ = 0;

  // The number of audio samples represented in a complete audio access
  // unit at dts_sampling_frequency.
  int frame_duration_ = 0;
};

}  // namespace mp4

}  // namespace media
#endif  // MEDIA_FORMATS_MP4_DTS_H_
