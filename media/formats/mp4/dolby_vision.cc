// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/dolby_vision.h"

#include "base/logging.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/box_reader.h"

namespace media {
namespace mp4 {

DolbyVisionConfiguration::DolbyVisionConfiguration() = default;
DolbyVisionConfiguration::~DolbyVisionConfiguration() = default;

FourCC DolbyVisionConfiguration::BoxType() const {
  return FOURCC_DVCC;
}

bool DolbyVisionConfiguration::Parse(BoxReader* reader) {
  return dovi_config.Parse(reader, reader->media_log()) &&
         dovi_config.dv_profile <= 7;
}

DolbyVisionConfiguration8::DolbyVisionConfiguration8() = default;
DolbyVisionConfiguration8::~DolbyVisionConfiguration8() = default;

FourCC DolbyVisionConfiguration8::BoxType() const {
  return FOURCC_DVVC;
}

bool DolbyVisionConfiguration8::Parse(BoxReader* reader) {
  return dovi_config.Parse(reader, reader->media_log()) &&
         dovi_config.dv_profile > 7 && dovi_config.dv_profile <= 20;
}

bool DOVIDecoderConfigurationRecord::ParseForTesting(const uint8_t* data,
                                                     int data_size) {
  BufferReader reader(data, data_size);
  NullMediaLog media_log;
  return Parse(&reader, &media_log);
}

bool DOVIDecoderConfigurationRecord::Parse(BufferReader* reader,
                                           MediaLog* media_log) {
  uint16_t profile_track_indication = 0;
  RCHECK(reader->Read1(&dv_version_major) && reader->Read1(&dv_version_minor) &&
         reader->Read2(&profile_track_indication));

  dv_profile = profile_track_indication >> 9;
  dv_level = (profile_track_indication >> 3) & 0x3F;
  rpu_present_flag = (profile_track_indication >> 2) & 1;
  el_present_flag = (profile_track_indication >> 1) & 1;
  bl_present_flag = profile_track_indication & 1;
  if (reader->HasBytes(1)) {
    RCHECK(reader->Read1(&dv_bl_signal_compatibility_id));
    dv_bl_signal_compatibility_id = (dv_bl_signal_compatibility_id >> 4) & 0x0F;
  }

  switch (dv_profile) {
    case 0:
      codec_profile = DOLBYVISION_PROFILE0;
      break;
    case 5:
      codec_profile = DOLBYVISION_PROFILE5;
      break;
    case 7:
      codec_profile = DOLBYVISION_PROFILE7;
      break;
    case 8:
      codec_profile = DOLBYVISION_PROFILE8;
      break;
    case 9:
      codec_profile = DOLBYVISION_PROFILE9;
      break;
    case 10:
      codec_profile = DOLBYVISION_PROFILE10;
      break;
    case 20:
      codec_profile = DOLBYVISION_PROFILE20;
      break;
    default:
      DVLOG(2) << "Deprecated or invalid Dolby Vision profile:"
               << static_cast<int>(dv_profile);
      return false;
  }

  DVLOG(2) << "Dolby Vision profile:" << static_cast<int>(dv_profile)
           << " level:" << static_cast<int>(dv_level)
           << " has_bl:" << static_cast<int>(bl_present_flag)
           << " has_el:" << static_cast<int>(el_present_flag)
           << " has_rpu:" << static_cast<int>(rpu_present_flag)
           << " bl_signal_compatibility_id: "
           << static_cast<int>(dv_bl_signal_compatibility_id)
           << " profile type:" << codec_profile;

  return true;
}

// Mappings are based on Dolby Vision Profiles and Levels Specification v1.5,
// section 2.1 ("Dolby Vision bitstream profiles").
VideoColorSpace ParseDolbyVisionColorSpace(VideoCodecProfile profile,
                                           uint8_t compatibility_id) {
  switch (profile) {
    case VideoCodecProfile::DOLBYVISION_PROFILE5:
      // Dolby Vision use IPT-PQ-C2 as matrix, but chromium has no definition
      // for it, so use BT.2020 NCL as a close approximation.
      return VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                             VideoColorSpace::TransferID::SMPTEST2084,
                             VideoColorSpace::MatrixID::BT2020_NCL,
                             gfx::ColorSpace::RangeID::FULL);
    case VideoCodecProfile::DOLBYVISION_PROFILE8:
      if (compatibility_id == kDolbyVisionCompatibilityIdHDR10) {
        return VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                               VideoColorSpace::TransferID::SMPTEST2084,
                               VideoColorSpace::MatrixID::BT2020_NCL,
                               gfx::ColorSpace::RangeID::LIMITED);
      } else if (compatibility_id == kDolbyVisionCompatibilityIdSDR) {
        return VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                               VideoColorSpace::TransferID::BT709,
                               VideoColorSpace::MatrixID::BT709,
                               gfx::ColorSpace::RangeID::LIMITED);
      } else if (compatibility_id == kDolbyVisionCompatibilityIdHLG) {
        return VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                               VideoColorSpace::TransferID::ARIB_STD_B67,
                               VideoColorSpace::MatrixID::BT2020_NCL,
                               gfx::ColorSpace::RangeID::LIMITED);
      }
      break;
    default:
      break;
  }
  return {};
}

}  // namespace mp4
}  // namespace media
