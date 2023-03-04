// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encode_accelerator.h"

#include <inttypes.h>

#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace media {

Vp9Metadata::Vp9Metadata() = default;
Vp9Metadata::~Vp9Metadata() = default;
Vp9Metadata::Vp9Metadata(const Vp9Metadata&) = default;

Av1Metadata::Av1Metadata() = default;
Av1Metadata::~Av1Metadata() = default;
Av1Metadata::Av1Metadata(const Av1Metadata&) = default;

BitstreamBufferMetadata::BitstreamBufferMetadata()
    : payload_size_bytes(0), key_frame(false) {}
BitstreamBufferMetadata::BitstreamBufferMetadata(
    const BitstreamBufferMetadata& other) = default;
BitstreamBufferMetadata& BitstreamBufferMetadata::operator=(
    const BitstreamBufferMetadata& other) = default;
BitstreamBufferMetadata::BitstreamBufferMetadata(
    BitstreamBufferMetadata&& other) = default;
BitstreamBufferMetadata::BitstreamBufferMetadata(size_t payload_size_bytes,
                                                 bool key_frame,
                                                 base::TimeDelta timestamp)
    : payload_size_bytes(payload_size_bytes),
      key_frame(key_frame),
      timestamp(timestamp) {}
BitstreamBufferMetadata::~BitstreamBufferMetadata() = default;

bool BitstreamBufferMetadata::end_of_picture() const {
  if (vp9) {
    return vp9->end_of_picture;
  }
  if (av1) {
    return av1->end_of_picture;
  }
  return true;
}

absl::optional<uint8_t> BitstreamBufferMetadata::spatial_idx() const {
  if (vp9) {
    return vp9->spatial_idx;
  }
  if (av1) {
    return av1->spatial_idx;
  }
  if (h265) {
    return h265->spatial_idx;
  }
  return absl::nullopt;
}

VideoEncodeAccelerator::Config::Config()
    : input_format(PIXEL_FORMAT_UNKNOWN),
      output_profile(VIDEO_CODEC_PROFILE_UNKNOWN),
      bitrate(Bitrate::ConstantBitrate(0u)),
      content_type(ContentType::kCamera) {}

VideoEncodeAccelerator::Config::Config(const Config& config) = default;

VideoEncodeAccelerator::Config::Config(
    VideoPixelFormat input_format,
    const gfx::Size& input_visible_size,
    VideoCodecProfile output_profile,
    const Bitrate& bitrate,
    absl::optional<uint32_t> initial_framerate,
    absl::optional<uint32_t> gop_length,
    absl::optional<uint8_t> h264_output_level,
    bool is_constrained_h264,
    absl::optional<StorageType> storage_type,
    ContentType content_type,
    const std::vector<SpatialLayer>& spatial_layers,
    InterLayerPredMode inter_layer_pred)
    : input_format(input_format),
      input_visible_size(input_visible_size),
      output_profile(output_profile),
      bitrate(bitrate),
      initial_framerate(initial_framerate.value_or(
          VideoEncodeAccelerator::kDefaultFramerate)),
      gop_length(gop_length),
      h264_output_level(h264_output_level),
      is_constrained_h264(is_constrained_h264),
      storage_type(storage_type),
      content_type(content_type),
      spatial_layers(spatial_layers),
      inter_layer_pred(inter_layer_pred) {}

VideoEncodeAccelerator::Config::~Config() = default;

std::string VideoEncodeAccelerator::Config::AsHumanReadableString() const {
  std::string str = base::StringPrintf(
      "input_format: %s, input_visible_size: %s, output_profile: %s, "
      "bitrate: %s",
      VideoPixelFormatToString(input_format).c_str(),
      input_visible_size.ToString().c_str(),
      GetProfileName(output_profile).c_str(), bitrate.ToString().c_str());
  if (initial_framerate) {
    str += base::StringPrintf(", initial_framerate: %u",
                              initial_framerate.value());
  }
  if (gop_length)
    str += base::StringPrintf(", gop_length: %u", gop_length.value());

  if (VideoCodecProfileToVideoCodec(output_profile) == VideoCodec::kH264) {
    if (h264_output_level) {
      str += base::StringPrintf(", h264_output_level: %u",
                                h264_output_level.value());
    }

    str += base::StringPrintf(", is_constrained_h264: %u", is_constrained_h264);
  }

  str += ", required_encoder_type: ";
  switch (required_encoder_type) {
    case EncoderType::kHardware:
      str += "hardware";
      break;
    case EncoderType::kSoftware:
      str += "software";
      break;
    case EncoderType::kNoPreference:
      str += "no-preference";
      break;
  }

  if (spatial_layers.empty())
    return str;

  for (size_t i = 0; i < spatial_layers.size(); ++i) {
    const auto& sl = spatial_layers[i];
    str += base::StringPrintf(
        ", {SpatialLayer#%zu: width=%" PRId32 ", height=%" PRId32
        ", bitrate_bps=%" PRIu32 ", framerate=%" PRId32
        ", max_qp=%u, num_of_temporal_layers=%u}",
        i, sl.width, sl.height, sl.bitrate_bps, sl.framerate, sl.max_qp,
        sl.num_of_temporal_layers);
  }

  str += ", InterLayerPredMode::";
  switch (inter_layer_pred) {
    case Config::InterLayerPredMode::kOff:
      str += "kOff";
      break;
    case Config::InterLayerPredMode::kOn:
      str += "kOn";
      break;
    case Config::InterLayerPredMode::kOnKeyPic:
      str += "kOnKeyPic";
      break;
  }

  return str;
}

bool VideoEncodeAccelerator::Config::HasTemporalLayer() const {
  return base::ranges::any_of(spatial_layers, [](const SpatialLayer& sl) {
    return sl.num_of_temporal_layers > 1u;
  });
}

bool VideoEncodeAccelerator::Config::HasSpatialLayer() const {
  return spatial_layers.size() > 1u;
}

void VideoEncodeAccelerator::Client::NotifyEncoderInfoChange(
    const VideoEncoderInfo& info) {
  // Do nothing if a client doesn't use the info.
}

VideoEncodeAccelerator::~VideoEncodeAccelerator() = default;

VideoEncodeAccelerator::SupportedProfile::SupportedProfile()
    : profile(media::VIDEO_CODEC_PROFILE_UNKNOWN) {}

VideoEncodeAccelerator::SupportedProfile::SupportedProfile(
    VideoCodecProfile profile,
    const gfx::Size& max_resolution,
    uint32_t max_framerate_numerator,
    uint32_t max_framerate_denominator,
    SupportedRateControlMode rc_modes,
    const std::vector<SVCScalabilityMode>& scalability_modes)
    : profile(profile),
      max_resolution(max_resolution),
      max_framerate_numerator(max_framerate_numerator),
      max_framerate_denominator(max_framerate_denominator),
      rate_control_modes(rc_modes),
      scalability_modes(scalability_modes) {}

VideoEncodeAccelerator::SupportedProfile::SupportedProfile(
    const SupportedProfile& other) = default;

VideoEncodeAccelerator::SupportedProfile::~SupportedProfile() = default;

void VideoEncodeAccelerator::Flush(FlushCallback flush_callback) {
  // TODO(owenlin): implements this https://crbug.com/755889.
  NOTIMPLEMENTED();
  std::move(flush_callback).Run(false);
}

bool VideoEncodeAccelerator::IsFlushSupported() {
  return false;
}

bool VideoEncodeAccelerator::IsGpuFrameResizeSupported() {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1166889) Add proper method overrides in
  // MojoVideoEncodeAccelerator and other subclasses that might return true.
  return true;
#else
  return false;
#endif
}

void VideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  RequestEncodingParametersChange(
      Bitrate::ConstantBitrate(bitrate_allocation.GetSumBps()), framerate);
}

bool operator==(const VideoEncodeAccelerator::SupportedProfile& l,
                const VideoEncodeAccelerator::SupportedProfile& r) {
  return l.profile == r.profile && l.min_resolution == r.min_resolution &&
         l.max_resolution == r.max_resolution &&
         l.max_framerate_numerator == r.max_framerate_numerator &&
         l.max_framerate_denominator == r.max_framerate_denominator &&
         l.rate_control_modes == r.rate_control_modes &&
         l.scalability_modes == r.scalability_modes &&
         l.is_software_codec == r.is_software_codec;
}

bool operator==(const H264Metadata& l, const H264Metadata& r) {
  return l.temporal_idx == r.temporal_idx && l.layer_sync == r.layer_sync;
}

bool operator==(const H265Metadata& l, const H265Metadata& r) {
  return l.temporal_idx == r.temporal_idx && l.spatial_idx == r.spatial_idx &&
         l.layer_sync == r.layer_sync;
}

bool operator==(const Vp8Metadata& l, const Vp8Metadata& r) {
  return l.non_reference == r.non_reference &&
         l.temporal_idx == r.temporal_idx && l.layer_sync == r.layer_sync;
}

bool operator==(const Vp9Metadata& l, const Vp9Metadata& r) {
  return l.inter_pic_predicted == r.inter_pic_predicted &&
         l.temporal_up_switch == r.temporal_up_switch &&
         l.referenced_by_upper_spatial_layers ==
             r.referenced_by_upper_spatial_layers &&
         l.reference_lower_spatial_layers == r.reference_lower_spatial_layers &&
         l.end_of_picture == r.end_of_picture &&
         l.temporal_idx == r.temporal_idx && l.spatial_idx == r.spatial_idx &&
         l.spatial_layer_resolutions == r.spatial_layer_resolutions &&
         l.p_diffs == r.p_diffs;
}

bool operator==(const Av1Metadata& l, const Av1Metadata& r) {
  return l.inter_pic_predicted == r.inter_pic_predicted &&
         l.switch_frame == r.switch_frame &&
         l.end_of_picture == r.end_of_picture &&
         l.temporal_idx == r.temporal_idx && l.spatial_idx == r.spatial_idx &&
         l.spatial_layer_resolutions == r.spatial_layer_resolutions &&
         l.f_diffs == r.f_diffs;
}

bool operator==(const BitstreamBufferMetadata& l,
                const BitstreamBufferMetadata& r) {
  return l.payload_size_bytes == r.payload_size_bytes &&
         l.key_frame == r.key_frame && l.timestamp == r.timestamp &&
         l.vp8 == r.vp8 && l.vp9 == r.vp9 && l.h264 == r.h264 &&
         l.av1 == r.av1 && l.h265 == r.h265;
}

bool operator==(const VideoEncodeAccelerator::Config::SpatialLayer& l,
                const VideoEncodeAccelerator::Config::SpatialLayer& r) {
  return l.width == r.width && l.height == r.height &&
         l.bitrate_bps == r.bitrate_bps && l.framerate == r.framerate &&
         l.max_qp == r.max_qp &&
         l.num_of_temporal_layers == r.num_of_temporal_layers;
}

bool operator==(const VideoEncodeAccelerator::Config& l,
                const VideoEncodeAccelerator::Config& r) {
  return l.input_format == r.input_format &&
         l.input_visible_size == r.input_visible_size &&
         l.output_profile == r.output_profile && l.bitrate == r.bitrate &&
         l.initial_framerate == r.initial_framerate &&
         l.gop_length == r.gop_length &&
         l.h264_output_level == r.h264_output_level &&
         l.storage_type == r.storage_type && l.content_type == r.content_type &&
         l.spatial_layers == r.spatial_layers &&
         l.inter_layer_pred == r.inter_layer_pred;
}
}  // namespace media

namespace std {

void default_delete<media::VideoEncodeAccelerator>::operator()(
    media::VideoEncodeAccelerator* vea) const {
  vea->Destroy();
}

}  // namespace std
