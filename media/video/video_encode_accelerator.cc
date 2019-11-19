// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encode_accelerator.h"

#include "base/callback.h"
#include "base/strings/stringprintf.h"

namespace media {

Vp8Metadata::Vp8Metadata()
    : non_reference(false), temporal_idx(0), layer_sync(false) {}
Vp8Metadata::Vp8Metadata(const Vp8Metadata& other) = default;
Vp8Metadata::Vp8Metadata(Vp8Metadata&& other) = default;
Vp8Metadata::~Vp8Metadata() = default;

BitstreamBufferMetadata::BitstreamBufferMetadata()
    : payload_size_bytes(0), key_frame(false) {}
BitstreamBufferMetadata::BitstreamBufferMetadata(
    BitstreamBufferMetadata&& other) = default;
BitstreamBufferMetadata::BitstreamBufferMetadata(size_t payload_size_bytes,
                                                 bool key_frame,
                                                 base::TimeDelta timestamp)
    : payload_size_bytes(payload_size_bytes),
      key_frame(key_frame),
      timestamp(timestamp) {}
BitstreamBufferMetadata::~BitstreamBufferMetadata() = default;

VideoEncodeAccelerator::Config::Config()
    : input_format(PIXEL_FORMAT_UNKNOWN),
      output_profile(VIDEO_CODEC_PROFILE_UNKNOWN),
      initial_bitrate(0),
      content_type(ContentType::kCamera) {}

VideoEncodeAccelerator::Config::Config(const Config& config) = default;

VideoEncodeAccelerator::Config::Config(
    VideoPixelFormat input_format,
    const gfx::Size& input_visible_size,
    VideoCodecProfile output_profile,
    uint32_t initial_bitrate,
    base::Optional<uint32_t> initial_framerate,
    base::Optional<uint32_t> gop_length,
    base::Optional<uint8_t> h264_output_level,
    base::Optional<StorageType> storage_type,
    ContentType content_type)
    : input_format(input_format),
      input_visible_size(input_visible_size),
      output_profile(output_profile),
      initial_bitrate(initial_bitrate),
      initial_framerate(initial_framerate.value_or(
          VideoEncodeAccelerator::kDefaultFramerate)),
      gop_length(gop_length),
      h264_output_level(h264_output_level),
      storage_type(storage_type),
      content_type(content_type) {}

VideoEncodeAccelerator::Config::~Config() = default;

std::string VideoEncodeAccelerator::Config::AsHumanReadableString() const {
  std::string str = base::StringPrintf(
      "input_format: %s, input_visible_size: %s, output_profile: %s, "
      "initial_bitrate: %u",
      VideoPixelFormatToString(input_format).c_str(),
      input_visible_size.ToString().c_str(),
      GetProfileName(output_profile).c_str(), initial_bitrate);
  if (initial_framerate) {
    str += base::StringPrintf(", initial_framerate: %u",
                              initial_framerate.value());
  }
  if (gop_length)
    str += base::StringPrintf(", gop_length: %u", gop_length.value());

  if (h264_output_level &&
      VideoCodecProfileToVideoCodec(output_profile) == kCodecH264) {
    str += base::StringPrintf(", h264_output_level: %u",
                              h264_output_level.value());
  }
  return str;
}

VideoEncodeAccelerator::~VideoEncodeAccelerator() = default;

VideoEncodeAccelerator::SupportedProfile::SupportedProfile()
    : profile(media::VIDEO_CODEC_PROFILE_UNKNOWN),
      max_framerate_numerator(0),
      max_framerate_denominator(0) {
}

VideoEncodeAccelerator::SupportedProfile::SupportedProfile(
    VideoCodecProfile profile,
    const gfx::Size& max_resolution,
    uint32_t max_framerate_numerator,
    uint32_t max_framerate_denominator)
    : profile(profile),
      max_resolution(max_resolution),
      max_framerate_numerator(max_framerate_numerator),
      max_framerate_denominator(max_framerate_denominator) {}

VideoEncodeAccelerator::SupportedProfile::~SupportedProfile() = default;

void VideoEncodeAccelerator::Flush(FlushCallback flush_callback) {
  // TODO(owenlin): implements this https://crbug.com/755889.
  NOTIMPLEMENTED();
  std::move(flush_callback).Run(false);
}

bool VideoEncodeAccelerator::IsFlushSupported() {
  return false;
}

void VideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  RequestEncodingParametersChange(bitrate_allocation.GetSumBps(), framerate);
}

}  // namespace media

namespace std {

void default_delete<media::VideoEncodeAccelerator>::operator()(
    media::VideoEncodeAccelerator* vea) const {
  vea->Destroy();
}

}  // namespace std
