// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_decoder_config.h"

#include <vector>

#include "base/logging.h"
#include "media/base/media_util.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"

namespace media {

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
      format_(PIXEL_FORMAT_UNKNOWN),
      rotation_(VIDEO_ROTATION_0) {}

VideoDecoderConfig::VideoDecoderConfig(
    VideoCodec codec,
    VideoCodecProfile profile,
    VideoPixelFormat format,
    ColorSpace color_space,
    VideoRotation rotation,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    const std::vector<uint8_t>& extra_data,
    const EncryptionScheme& encryption_scheme) {
  Initialize(codec, profile, format, color_space, rotation, coded_size,
             visible_rect, natural_size, extra_data, encryption_scheme);
}

VideoDecoderConfig::VideoDecoderConfig(const VideoDecoderConfig& other) =
    default;

VideoDecoderConfig::~VideoDecoderConfig() = default;

void VideoDecoderConfig::set_color_space_info(
    const VideoColorSpace& color_space_info) {
  color_space_info_ = color_space_info;
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
                                    VideoPixelFormat format,
                                    ColorSpace color_space,
                                    VideoRotation rotation,
                                    const gfx::Size& coded_size,
                                    const gfx::Rect& visible_rect,
                                    const gfx::Size& natural_size,
                                    const std::vector<uint8_t>& extra_data,
                                    const EncryptionScheme& encryption_scheme) {
  codec_ = codec;
  profile_ = profile;
  format_ = format;
  rotation_ = rotation;
  coded_size_ = coded_size;
  visible_rect_ = visible_rect;
  natural_size_ = natural_size;
  extra_data_ = extra_data;
  encryption_scheme_ = encryption_scheme;

  switch (color_space) {
    case ColorSpace::COLOR_SPACE_JPEG:
      color_space_info_ = VideoColorSpace::JPEG();
      break;
    case ColorSpace::COLOR_SPACE_HD_REC709:
      color_space_info_ = VideoColorSpace::REC709();
      break;
    case ColorSpace::COLOR_SPACE_SD_REC601:
      color_space_info_ = VideoColorSpace::REC601();
      break;
    case ColorSpace::COLOR_SPACE_UNSPECIFIED:
    default:
      break;
  }
}

bool VideoDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownVideoCodec && natural_size_.width() > 0 &&
         natural_size_.height() > 0 &&
         VideoFrame::IsValidConfig(format_, VideoFrame::STORAGE_UNOWNED_MEMORY,
                                   coded_size_, visible_rect_, natural_size_);
}

bool VideoDecoderConfig::Matches(const VideoDecoderConfig& config) const {
  return ((codec() == config.codec()) && (format() == config.format()) &&
          (profile() == config.profile()) &&
          (video_rotation() == config.video_rotation()) &&
          (coded_size() == config.coded_size()) &&
          (visible_rect() == config.visible_rect()) &&
          (natural_size() == config.natural_size()) &&
          (extra_data() == config.extra_data()) &&
          (encryption_scheme().Matches(config.encryption_scheme())) &&
          (color_space_info() == config.color_space_info()) &&
          (hdr_metadata() == config.hdr_metadata()));
}

std::string VideoDecoderConfig::AsHumanReadableString() const {
  std::ostringstream s;
  s << "codec: " << GetCodecName(codec()) << ", format: " << format()
    << ", profile: " << GetProfileName(profile()) << ", coded size: ["
    << coded_size().width() << "," << coded_size().height() << "]"
    << ", visible rect: [" << visible_rect().x() << "," << visible_rect().y()
    << "," << visible_rect().width() << "," << visible_rect().height() << "]"
    << ", natural size: [" << natural_size().width() << ","
    << natural_size().height() << "]"
    << ", has extra data: " << (extra_data().empty() ? "false" : "true")
    << ", encryption scheme: " << encryption_scheme()
    << ", rotation: " << VideoRotationToString(video_rotation());
  return s.str();
}

double VideoDecoderConfig::GetPixelAspectRatio() const {
  return ::media::GetPixelAspectRatio(visible_rect_, natural_size_);
}

void VideoDecoderConfig::SetExtraData(const std::vector<uint8_t>& extra_data) {
  extra_data_ = extra_data;
}

void VideoDecoderConfig::SetIsEncrypted(bool is_encrypted) {
  if (!is_encrypted) {
    DCHECK(encryption_scheme_.is_encrypted()) << "Config is already clear.";
    encryption_scheme_ = Unencrypted();
  } else {
    DCHECK(!encryption_scheme_.is_encrypted())
        << "Config is already encrypted.";
    // TODO(xhwang): This is only used to guide decoder selection, so set
    // a common encryption scheme that should be supported by all decrypting
    // decoders. We should be able to remove this when we support switching
    // decoders at run time. See http://crbug.com/695595
    encryption_scheme_ = AesCtrEncryptionScheme();
  }
}

}  // namespace media
