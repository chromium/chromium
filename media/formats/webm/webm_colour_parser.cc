// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_colour_parser.h"

#include "base/check.h"
#include "base/logging.h"
#include "media/formats/webm/webm_constants.h"
#include "third_party/libwebm/source/mkvmuxer/mkvmuxer.h"

namespace media {

WebMColorMetadata::WebMColorMetadata() = default;
WebMColorMetadata::WebMColorMetadata(const WebMColorMetadata& rhs) = default;

WebMColorVolumeMetadataParser::WebMColorVolumeMetadataParser() = default;
WebMColorVolumeMetadataParser::~WebMColorVolumeMetadataParser() = default;

bool WebMColorVolumeMetadataParser::OnFloat(int id, double val) {
  switch (id) {
    case kWebMIdPrimaryRChromaticityX:
      color_volume_metadata_.primaries.fRX = val;
      break;
    case kWebMIdPrimaryRChromaticityY:
      color_volume_metadata_.primaries.fRY = val;
      break;
    case kWebMIdPrimaryGChromaticityX:
      color_volume_metadata_.primaries.fGX = val;
      break;
    case kWebMIdPrimaryGChromaticityY:
      color_volume_metadata_.primaries.fGY = val;
      break;
    case kWebMIdPrimaryBChromaticityX:
      color_volume_metadata_.primaries.fBX = val;
      break;
    case kWebMIdPrimaryBChromaticityY:
      color_volume_metadata_.primaries.fBY = val;
      break;
    case kWebMIdWhitePointChromaticityX:
      color_volume_metadata_.primaries.fWX = val;
      break;
    case kWebMIdWhitePointChromaticityY:
      color_volume_metadata_.primaries.fWY = val;
      break;
    case kWebMIdLuminanceMax:
      color_volume_metadata_.luminance_max = val;
      break;
    case kWebMIdLuminanceMin:
      color_volume_metadata_.luminance_min = val;
      break;
    default:
      DVLOG(1) << "Unexpected id in ColorVolumeMetadata: 0x" << std::hex << id;
      return false;
  }
  return true;
}

WebMColourParser::WebMColourParser() {
  Reset();
}

WebMColourParser::~WebMColourParser() = default;

void WebMColourParser::Reset() {
  matrix_coefficients_ = -1;
  bits_per_channel_ = -1;
  chroma_subsampling_horz_ = -1;
  chroma_subsampling_vert_ = -1;
  cb_subsampling_horz_ = -1;
  cb_subsampling_vert_ = -1;
  chroma_siting_horz_ = -1;
  chroma_siting_vert_ = -1;
  range_ = -1;
  transfer_characteristics_ = -1;
  primaries_ = -1;
  max_content_light_level_ = -1;
  max_frame_average_light_level_ = -1;
}

WebMParserClient* WebMColourParser::OnListStart(int id) {
  if (id == kWebMIdColorVolumeMetadata) {
    color_volume_metadata_parsed_ = false;
    return &color_volume_metadata_parser_;
  }

  return this;
}

bool WebMColourParser::OnListEnd(int id) {
  if (id == kWebMIdColorVolumeMetadata)
    color_volume_metadata_parsed_ = true;
  return true;
}

bool WebMColourParser::OnUInt(int id, int64_t val) {
  int64_t* dst = nullptr;

  switch (id) {
    case kWebMIdMatrixCoefficients:
      dst = &matrix_coefficients_;
      break;
    case kWebMIdBitsPerChannel:
      dst = &bits_per_channel_;
      break;
    case kWebMIdChromaSubsamplingHorz:
      dst = &chroma_subsampling_horz_;
      break;
    case kWebMIdChromaSubsamplingVert:
      dst = &chroma_subsampling_vert_;
      break;
    case kWebMIdCbSubsamplingHorz:
      dst = &cb_subsampling_horz_;
      break;
    case kWebMIdCbSubsamplingVert:
      dst = &cb_subsampling_vert_;
      break;
    case kWebMIdChromaSitingHorz:
      dst = &chroma_siting_horz_;
      break;
    case kWebMIdChromaSitingVert:
      dst = &chroma_siting_vert_;
      break;
    case kWebMIdRange:
      dst = &range_;
      break;
    case kWebMIdTransferCharacteristics:
      dst = &transfer_characteristics_;
      break;
    case kWebMIdPrimaries:
      dst = &primaries_;
      break;
    case kWebMIdMaxCLL:
      dst = &max_content_light_level_;
      break;
    case kWebMIdMaxFALL:
      dst = &max_frame_average_light_level_;
      break;
    default:
      return true;
  }

  DCHECK(dst);
  if (*dst != -1) {
    LOG(ERROR) << "Multiple values for id " << std::hex << id << " specified ("
               << *dst << " and " << val << ")";
    return false;
  }

  *dst = val;
  return true;
}

WebMColorMetadata WebMColourParser::GetWebMColorMetadata() const {
  WebMColorMetadata color_metadata;

  if (bits_per_channel_ != -1)
    color_metadata.BitsPerChannel = bits_per_channel_;

  if (chroma_subsampling_horz_ != -1)
    color_metadata.ChromaSubsamplingHorz = chroma_subsampling_horz_;
  if (chroma_subsampling_vert_ != -1)
    color_metadata.ChromaSubsamplingVert = chroma_subsampling_vert_;
  if (cb_subsampling_horz_ != -1)
    color_metadata.CbSubsamplingHorz = cb_subsampling_horz_;
  if (cb_subsampling_vert_ != -1)
    color_metadata.CbSubsamplingVert = cb_subsampling_vert_;
  if (chroma_siting_horz_ != -1)
    color_metadata.ChromaSitingHorz = chroma_siting_horz_;
  if (chroma_siting_vert_ != -1)
    color_metadata.ChromaSitingVert = chroma_siting_vert_;

  gfx::ColorSpace::RangeID range_id = gfx::ColorSpace::RangeID::INVALID;
  if (range_ >= static_cast<int64_t>(mkvmuxer::Colour::kUnspecifiedCr) &&
      range_ <= static_cast<int64_t>(mkvmuxer::Colour::kMcTcDefined)) {
    switch (range_) {
      case mkvmuxer::Colour::kUnspecifiedCr:
        range_id = gfx::ColorSpace::RangeID::INVALID;
        break;
      case mkvmuxer::Colour::kBroadcastRange:
        range_id = gfx::ColorSpace::RangeID::LIMITED;
        break;
      case mkvmuxer::Colour::kFullRange:
        range_id = gfx::ColorSpace::RangeID::FULL;
        break;
      case mkvmuxer::Colour::kMcTcDefined:
        range_id = gfx::ColorSpace::RangeID::DERIVED;
        break;
    }
  }
  color_metadata.color_space = VideoColorSpace(
      primaries_, transfer_characteristics_, matrix_coefficients_, range_id);

  if (max_content_light_level_ != -1 || max_frame_average_light_level_ != -1 ||
      color_volume_metadata_parsed_) {
    color_metadata.hdr_metadata = gfx::HDRMetadata();

    if (max_content_light_level_ != -1) {
      color_metadata.hdr_metadata->max_content_light_level =
          max_content_light_level_;
    }

    if (max_frame_average_light_level_ != -1) {
      color_metadata.hdr_metadata->max_frame_average_light_level =
          max_frame_average_light_level_;
    }

    if (color_volume_metadata_parsed_) {
      color_metadata.hdr_metadata->color_volume_metadata =
          color_volume_metadata_parser_.GetColorVolumeMetadata();
    }
  }

  return color_metadata;
}

}  // namespace media
