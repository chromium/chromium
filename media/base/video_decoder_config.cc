// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_decoder_config.h"

#include <iomanip>
#include <vector>

#include "base/logging.h"
#include "media/base/limits.h"
#include "media/base/media_util.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"

namespace media {

static bool IsValidSize(const gfx::Size& size) {
  const int area = size.GetCheckedArea().ValueOrDefault(INT_MAX);
  return area && area <= limits::kMaxCanvas &&
         size.width() <= limits::kMaxDimension &&
         size.height() <= limits::kMaxDimension;
}

VideoCodec VideoCodecProfileToVideoCodec(VideoCodecProfile profile) {
  switch (profile) {
    case VIDEO_CODEC_PROFILE_UNKNOWN:
      return kUnknownVideoCodec;
    case H264PROFILE_BASELINE:
    case H264PROFILE_MAIN:
    case H264PROFILE_EXTENDED:
    case H264PROFILE_HIGH:
    case H264PROFILE_HIGH10PROFILE:
    case H264PROFILE_HIGH422PROFILE:
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
    case H264PROFILE_SCALABLEBASELINE:
    case H264PROFILE_SCALABLEHIGH:
    case H264PROFILE_STEREOHIGH:
    case H264PROFILE_MULTIVIEWHIGH:
      return kCodecH264;
    case HEVCPROFILE_MAIN:
    case HEVCPROFILE_MAIN10:
    case HEVCPROFILE_MAIN_STILL_PICTURE:
      return kCodecHEVC;
    case VP8PROFILE_ANY:
      return kCodecVP8;
    case VP9PROFILE_PROFILE0:
    case VP9PROFILE_PROFILE1:
    case VP9PROFILE_PROFILE2:
    case VP9PROFILE_PROFILE3:
      return kCodecVP9;
    case DOLBYVISION_PROFILE0:
    case DOLBYVISION_PROFILE4:
    case DOLBYVISION_PROFILE5:
    case DOLBYVISION_PROFILE7:
    case DOLBYVISION_PROFILE8:
    case DOLBYVISION_PROFILE9:
      return kCodecDolbyVision;
    case THEORAPROFILE_ANY:
      return kCodecTheora;
    case AV1PROFILE_PROFILE_MAIN:
    case AV1PROFILE_PROFILE_HIGH:
    case AV1PROFILE_PROFILE_PRO:
      return kCodecAV1;
  }
  NOTREACHED();
  return kUnknownVideoCodec;
}

VideoDecoderConfig::VideoDecoderConfig()
    : codec_(kUnknownVideoCodec),
      profile_(VIDEO_CODEC_PROFILE_UNKNOWN),
      alpha_mode_(AlphaMode::kIsOpaque),
      transformation_(kNoTransformation) {}

VideoDecoderConfig::VideoDecoderConfig(VideoCodec codec,
                                       VideoCodecProfile profile,
                                       AlphaMode alpha_mode,
                                       const VideoColorSpace& color_space,
                                       VideoTransformation rotation,
                                       const gfx::Size& coded_size,
                                       const gfx::Rect& visible_rect,
                                       const gfx::Size& natural_size,
                                       const std::vector<uint8_t>& extra_data,
                                       EncryptionScheme encryption_scheme) {
  Initialize(codec, profile, alpha_mode, color_space, rotation, coded_size,
             visible_rect, natural_size, extra_data, encryption_scheme);
}

VideoDecoderConfig::VideoDecoderConfig(const VideoDecoderConfig& other) =
    default;

VideoDecoderConfig::~VideoDecoderConfig() = default;

void VideoDecoderConfig::set_color_space_info(
    const VideoColorSpace& color_space) {
  color_space_info_ = color_space;
}

const VideoColorSpace& VideoDecoderConfig::color_space_info() const {
  return color_space_info_;
}

void VideoDecoderConfig::set_hdr_metadata(const HDRMetadata& hdr_metadata) {
  hdr_metadata_ = hdr_metadata;
}

const base::Optional<HDRMetadata>& VideoDecoderConfig::hdr_metadata() const {
  return hdr_metadata_;
}

void VideoDecoderConfig::Initialize(VideoCodec codec,
                                    VideoCodecProfile profile,
                                    AlphaMode alpha_mode,
                                    const VideoColorSpace& color_space,
                                    VideoTransformation transformation,
                                    const gfx::Size& coded_size,
                                    const gfx::Rect& visible_rect,
                                    const gfx::Size& natural_size,
                                    const std::vector<uint8_t>& extra_data,
                                    EncryptionScheme encryption_scheme) {
  codec_ = codec;
  profile_ = profile;
  alpha_mode_ = alpha_mode;
  transformation_ = transformation;
  coded_size_ = coded_size;
  visible_rect_ = visible_rect;
  natural_size_ = natural_size;
  extra_data_ = extra_data;
  encryption_scheme_ = encryption_scheme;
  color_space_info_ = color_space;
}

bool VideoDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownVideoCodec && IsValidSize(coded_size_) &&
         IsValidSize(natural_size_) &&
         gfx::Rect(coded_size_).Contains(visible_rect_);
}

bool VideoDecoderConfig::Matches(const VideoDecoderConfig& config) const {
  return codec() == config.codec() && profile() == config.profile() &&
         alpha_mode() == config.alpha_mode() &&
         video_transformation() == config.video_transformation() &&
         coded_size() == config.coded_size() &&
         visible_rect() == config.visible_rect() &&
         natural_size() == config.natural_size() &&
         extra_data() == config.extra_data() &&
         encryption_scheme() == config.encryption_scheme() &&
         color_space_info() == config.color_space_info() &&
         hdr_metadata() == config.hdr_metadata();
}

std::string VideoDecoderConfig::AsHumanReadableString() const {
  std::ostringstream s;
  s << "codec: " << GetCodecName(codec())
    << ", profile: " << GetProfileName(profile()) << ", alpha_mode: "
    << (alpha_mode() == AlphaMode::kHasAlpha ? "has_alpha" : "is_opaque")
    << ", coded size: [" << coded_size().width() << "," << coded_size().height()
    << "]"
    << ", visible rect: [" << visible_rect().x() << "," << visible_rect().y()
    << "," << visible_rect().width() << "," << visible_rect().height() << "]"
    << ", natural size: [" << natural_size().width() << ","
    << natural_size().height() << "]"
    << ", has extra data: " << (extra_data().empty() ? "false" : "true")
    << ", encryption scheme: " << encryption_scheme()
    << ", rotation: " << VideoRotationToString(video_transformation().rotation)
    << ", flipped: " << video_transformation().mirrored
    << ", color space: " << color_space_info().ToGfxColorSpace().ToString();
  if (hdr_metadata().has_value()) {
    s << std::setprecision(4) << ", luminance range: "
      << hdr_metadata()->mastering_metadata.luminance_min << "-"
      << hdr_metadata()->mastering_metadata.luminance_max << ", primaries: r("
      << hdr_metadata()->mastering_metadata.primary_r.x() << ","
      << hdr_metadata()->mastering_metadata.primary_r.y() << ") g("
      << hdr_metadata()->mastering_metadata.primary_g.x() << ","
      << hdr_metadata()->mastering_metadata.primary_g.y() << ") b("
      << hdr_metadata()->mastering_metadata.primary_b.x() << ","
      << hdr_metadata()->mastering_metadata.primary_b.y() << ") wp("
      << hdr_metadata()->mastering_metadata.white_point.x() << ","
      << hdr_metadata()->mastering_metadata.white_point.y() << ")";
  }
  return s.str();
}

std::string VideoDecoderConfig::GetHumanReadableCodecName() const {
  return GetCodecName(codec());
}

double VideoDecoderConfig::GetPixelAspectRatio() const {
  return ::media::GetPixelAspectRatio(visible_rect_, natural_size_);
}

void VideoDecoderConfig::SetExtraData(const std::vector<uint8_t>& extra_data) {
  extra_data_ = extra_data;
}

void VideoDecoderConfig::SetIsEncrypted(bool is_encrypted) {
  if (!is_encrypted) {
    DCHECK_NE(encryption_scheme_, EncryptionScheme::kUnencrypted)
        << "Config is already clear.";
    encryption_scheme_ = EncryptionScheme::kUnencrypted;
  } else {
    DCHECK_EQ(encryption_scheme_, EncryptionScheme::kUnencrypted)
        << "Config is already encrypted.";
    // TODO(xhwang): This is only used to guide decoder selection, so set
    // a common encryption scheme that should be supported by all decrypting
    // decoders. We should be able to remove this when we support switching
    // decoders at run time. See http://crbug.com/695595
    encryption_scheme_ = EncryptionScheme::kCenc;
  }
}

}  // namespace media
