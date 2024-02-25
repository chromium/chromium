// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_AC4_H_
#define MEDIA_FORMATS_MP4_AC4_H_

#include <stdint.h>

#include <vector>

#include "build/build_config.h"
#include "media/base/audio_codecs.h"
#include "media/base/bit_reader.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/media_buildflags.h"

namespace media {

namespace mp4 {

struct AC4StreamInfo {
  uint8_t bitstream_version;
  uint8_t presentation_version;
  uint8_t presentation_level;
  uint8_t is_ajoc;
  uint8_t is_ims;
  uint8_t channels;
};

// This class parses the AC4 information from decoder specific information
// embedded in the dac4 box in an ISO BMFF file.
// Please refer to ETSI TS 103 190-2 V1.2.1 (2018-02)
//     https://www.etsi.org/deliver/etsi_ts/103100_103199/10319002/01.02.01_60/ts_10319002v010201p.pdf
//     E.5 AC4SpecificBox
// for more details.
// For IMS, Please refer to
//     https://ott.dolby.com/OnDelKits/AC-4/Dolby_AC-4_Online_Delivery_Kit_1.5/Documentation/Specs/AC4_DASH/help_files/topics/ac4_in_mpeg_dash_c_signaling_ac4_in_iso.html
class MEDIA_EXPORT AC4 {
 public:
  AC4();
  AC4(const AC4& other);
  ~AC4();

  bool Parse(const std::vector<uint8_t>& data, MediaLog* media_log);
  std::vector<uint8_t> StreamInfo() const;

 private:
  // Use to parse AC4 DSI when dsi version is 1
  bool ParseAc4DsiV1(BitReader& reader);
  bool ParseAc4PresentationV1Dsi(BitReader& reader,
                                 int pres_bytes,
                                 int& consumed_pres_bytes,
                                 uint8_t bitstream_version,
                                 uint8_t presentation_version);
  bool ParseAc4SubstreamGroupDsi(BitReader& reader,
                                 uint8_t bitstream_version,
                                 uint8_t presentation_version,
                                 uint8_t presentation_level);
  void LogAC4Parameters();

  std::vector<AC4StreamInfo> stream_info_internals_;
};

}  // namespace mp4

}  // namespace media

#endif  // MEDIA_FORMATS_MP4_AC4_H_
