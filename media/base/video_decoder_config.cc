// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_decoder_config.h"

#include <iomanip>
#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
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

VideoDecoderConfig::VideoDecoderConfig() = default;

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

VideoDecoderConfig::VideoDecoderConfig(VideoDecoderConfig&& other) = default;

VideoDecoderConfig& VideoDecoderConfig::operator=(
    const VideoDecoderConfig& other) = default;

VideoDecoderConfig& VideoDecoderConfig::operator=(VideoDecoderConfig&& other) =
    default;

VideoDecoderConfig::~VideoDecoderConfig() = default;

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
  aspect_ratio_ = VideoAspectRatio(visible_rect, natural_size);
  extra_data_ = extra_data;
  encryption_scheme_ = encryption_scheme;
  color_space_info_ = color_space;
}

bool VideoDecoderConfig::IsValidConfig() const {
  return codec_ != VideoCodec::kUnknown && IsValidSize(coded_size_) &&
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
         aspect_ratio() == config.aspect_ratio() &&
         extra_data() == config.extra_data() &&
         encryption_scheme() == config.encryption_scheme() &&
         color_space_info() == config.color_space_info() &&
         hdr_metadata() == config.hdr_metadata() && level() == config.level();
}

std::string VideoDecoderConfig::AsHumanReadableString() const {
  std::ostringstream s;
  s << "codec: " << GetCodecName(codec())
    << ", profile: " << GetProfileName(profile()) << ", level: "
    << (level() > kNoVideoCodecLevel ? base::NumberToString(level())
                                     : "not available")
    << ", alpha_mode: "
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
    s << ", hdr metadata: " << hdr_metadata()->ToString();
  }

  return s.str();
}

std::string VideoDecoderConfig::GetHumanReadableCodecName() const {
  return GetCodecName(codec());
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
