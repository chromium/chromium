// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_DOLBY_VISION_H_
#define MEDIA_FORMATS_MP4_DOLBY_VISION_H_

#include "media/base/media_export.h"
#include "media/formats/mp4/box_definitions.h"

namespace media {

namespace mp4 {

struct MEDIA_EXPORT DOVIDecoderConfigurationRecord {
  uint8_t dv_version_major = 0;
  uint8_t dv_version_minor = 0;
  uint8_t dv_profile = 0;
  uint8_t dv_level = 0;
  uint8_t rpu_present_flag = 0;
  uint8_t el_present_flag = 0;
  uint8_t bl_present_flag = 0;
  uint8_t dv_bl_signal_compatibility_id = 0;

  VideoCodecProfile codec_profile = VIDEO_CODEC_PROFILE_UNKNOWN;

  bool Parse(BufferReader* reader, MediaLog* media_log);

  // Parses DolbyVisionConfiguration data encoded in |data|.
  // Note: This method is intended to parse data outside the MP4StreamParser
  //       context and therefore the box header is not expected to be present
  //       in |data|.
  // Returns true if |data| was successfully parsed.
  bool ParseForTesting(const uint8_t* data, int data_size);
};

// The structures of the configuration is defined in Dolby Streams Within the
// ISO Base Media File Format v2.0 section 3.1.

// dvcC, used for profile 7 and earlier.
struct MEDIA_EXPORT DolbyVisionConfiguration : Box {
  DECLARE_BOX_METHODS(DolbyVisionConfiguration);

  DOVIDecoderConfigurationRecord dovi_config;
};

// dvvC, used for profile 8 and later.
struct MEDIA_EXPORT DolbyVisionConfiguration8 : Box {
  DECLARE_BOX_METHODS(DolbyVisionConfiguration8);

  DOVIDecoderConfigurationRecord dovi_config;
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_DOLBY_VISION_H_
