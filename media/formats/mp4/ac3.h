// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_AC3_H_
#define MEDIA_FORMATS_MP4_AC3_H_

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

// This class parses the EAC3 information from decoder specific information
// embedded in the dec3 box in an ISO BMFF file.
// Please refer to ETSI TS 102 366 V1.4.1
//     https://www.etsi.org/deliver/etsi_ts/102300_102399/102366/01.04.01_60/ts_102366v010401p.pdf
//     F.4 AC3SpecificBox
// for more details.
class MEDIA_EXPORT AC3 {
 public:
  AC3();
  AC3(const AC3& other);
  ~AC3();

  // Parse the AC3 config from the dac3 box.
  bool Parse(const std::vector<uint8_t>& data, MediaLog* media_log);

  uint32_t GetChannelCount() const;
  ChannelLayout GetChannelLayout() const;

 private:
  // The channel count stored in the compressed audio stream.
  uint32_t channel_count_ = 0;
  ChannelLayout channel_layout_ = CHANNEL_LAYOUT_UNSUPPORTED;
};

}  // namespace mp4

}  // namespace media

#endif  // MEDIA_FORMATS_MP4_AC3_H_
