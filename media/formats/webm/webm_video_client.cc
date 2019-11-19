// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_video_client.h"

#include "media/base/video_decoder_config.h"
#include "media/formats/webm/webm_constants.h"
#include "media/media_buildflags.h"

namespace media {

namespace {

// Tries to parse |data| to extract the VP9 Profile ID, or returns Profile 0.
media::VideoCodecProfile GetVP9CodecProfile(const std::vector<uint8_t>& data,
                                            bool is_probably_10bit) {
  // VP9 CodecPrivate (http://wiki.webmproject.org/vp9-codecprivate) might have
  // Profile information in the first field, if present.
  constexpr uint8_t kVP9ProfileFieldId = 0x01;
  constexpr uint8_t kVP9ProfileFieldLength = 1;
  if (data.size() < 3 || data[0] != kVP9ProfileFieldId ||
      data[1] != kVP9ProfileFieldLength || data[2] > 3) {
    return is_probably_10bit ? VP9PROFILE_PROFILE2 : VP9PROFILE_PROFILE0;
  }

  return static_cast<VideoCodecProfile>(
      static_cast<size_t>(VP9PROFILE_PROFILE0) + data[2]);
}

}  // namespace

WebMVideoClient::WebMVideoClient(MediaLog* media_log) : media_log_(media_log) {
  Reset();
}

WebMVideoClient::~WebMVideoClient() = default;

void WebMVideoClient::Reset() {
  pixel_width_ = -1;
  pixel_height_ = -1;
  crop_bottom_ = -1;
  crop_top_ = -1;
  crop_left_ = -1;
  crop_right_ = -1;
  display_width_ = -1;
  display_height_ = -1;
  display_unit_ = -1;
  alpha_mode_ = -1;
  colour_parsed_ = false;
}

bool WebMVideoClient::InitializeConfig(
    const std::string& codec_id,
    const std::vector<uint8_t>& codec_private,
    EncryptionScheme encryption_scheme,
    VideoDecoderConfig* config) {
  DCHECK(config);

  bool is_8bit = true;
  VideoColorSpace color_space = VideoColorSpace::REC709();
  if (colour_parsed_) {
    WebMColorMetadata color_metadata = colour_parser_.GetWebMColorMetadata();
    color_space = color_metadata.color_space;
    if (color_metadata.hdr_metadata.has_value())
      config->set_hdr_metadata(*color_metadata.hdr_metadata);
    is_8bit = color_metadata.BitsPerChannel <= 8;
  }

  VideoCodec video_codec = kUnknownVideoCodec;
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  if (codec_id == "V_VP8") {
    video_codec = kCodecVP8;
    profile = VP8PROFILE_ANY;
  } else if (codec_id == "V_VP9") {
    video_codec = kCodecVP9;
    profile = GetVP9CodecProfile(
        codec_private, color_space.ToGfxColorSpace().IsHDR() ||
                           config->hdr_metadata().has_value() || !is_8bit);
#if BUILDFLAG(ENABLE_AV1_DECODER)
  } else if (codec_id == "V_AV1") {
    // TODO(dalecurtis): AV1 profiles in WebM are not finalized, this needs
    // updating to read the actual profile and configuration before enabling for
    // release. http://crbug.com/784993
    video_codec = kCodecAV1;
    profile = AV1PROFILE_PROFILE_MAIN;
#endif
  } else {
    MEDIA_LOG(ERROR, media_log_) << "Unsupported video codec_id " << codec_id;
    return false;
  }

  if (pixel_width_ <= 0 || pixel_height_ <= 0)
    return false;

  // Set crop and display unit defaults if these elements are not present.
  if (crop_bottom_ == -1)
    crop_bottom_ = 0;

  if (crop_top_ == -1)
    crop_top_ = 0;

  if (crop_left_ == -1)
    crop_left_ = 0;

  if (crop_right_ == -1)
    crop_right_ = 0;

  if (display_unit_ == -1)
    display_unit_ = 0;

  gfx::Size coded_size(pixel_width_, pixel_height_);
  gfx::Rect visible_rect(crop_top_, crop_left_,
                         pixel_width_ - (crop_left_ + crop_right_),
                         pixel_height_ - (crop_top_ + crop_bottom_));
  // TODO(dalecurtis): This is not correct, but it's what's muxed in webm
  // containers with AV1 right now. So accept it. We won't get here unless the
  // build and runtime flags are enabled for AV1.
  if (display_unit_ == 0 || (video_codec == kCodecAV1 && display_unit_ == 4)) {
    if (display_width_ <= 0)
      display_width_ = visible_rect.width();
    if (display_height_ <= 0)
      display_height_ = visible_rect.height();
  } else if (display_unit_ == 3) {
    if (display_width_ <= 0 || display_height_ <= 0)
      return false;
  } else {
    MEDIA_LOG(ERROR, media_log_)
        << "Unsupported display unit type " << display_unit_;
    return false;
  }
  gfx::Size natural_size = gfx::Size(display_width_, display_height_);

  config->Initialize(video_codec, profile,
                     alpha_mode_ == 1
                         ? VideoDecoderConfig::AlphaMode::kHasAlpha
                         : VideoDecoderConfig::AlphaMode::kIsOpaque,
                     color_space, kNoTransformation, coded_size, visible_rect,
                     natural_size, codec_private, encryption_scheme);
  return config->IsValidConfig();
}

WebMParserClient* WebMVideoClient::OnListStart(int id) {
  if (id == kWebMIdColour) {
    colour_parsed_ = false;
    return &colour_parser_;
  }

  return this;
}

bool WebMVideoClient::OnListEnd(int id) {
  if (id == kWebMIdColour)
    colour_parsed_ = true;
  return true;
}

bool WebMVideoClient::OnUInt(int id, int64_t val) {
  int64_t* dst = NULL;

  switch (id) {
    case kWebMIdPixelWidth:
      dst = &pixel_width_;
      break;
    case kWebMIdPixelHeight:
      dst = &pixel_height_;
      break;
    case kWebMIdPixelCropTop:
      dst = &crop_top_;
      break;
    case kWebMIdPixelCropBottom:
      dst = &crop_bottom_;
      break;
    case kWebMIdPixelCropLeft:
      dst = &crop_left_;
      break;
    case kWebMIdPixelCropRight:
      dst = &crop_right_;
      break;
    case kWebMIdDisplayWidth:
      dst = &display_width_;
      break;
    case kWebMIdDisplayHeight:
      dst = &display_height_;
      break;
    case kWebMIdDisplayUnit:
      dst = &display_unit_;
      break;
    case kWebMIdAlphaMode:
      dst = &alpha_mode_;
      break;
    default:
      return true;
  }

  if (*dst != -1) {
    MEDIA_LOG(ERROR, media_log_) << "Multiple values for id " << std::hex << id
                                 << " specified (" << *dst << " and " << val
                                 << ")";
    return false;
  }

  *dst = val;
  return true;
}

bool WebMVideoClient::OnBinary(int id, const uint8_t* data, int size) {
  // Accept binary fields we don't care about for now.
  return true;
}

bool WebMVideoClient::OnFloat(int id, double val) {
  // Accept float fields we don't care about for now.
  return true;
}

}  // namespace media
