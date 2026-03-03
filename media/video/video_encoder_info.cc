// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encoder_info.h"

#include <algorithm>
#include <tuple>

#include "build/build_config.h"

namespace media {

ResolutionRateLimit::ResolutionRateLimit() = default;
ResolutionRateLimit::ResolutionRateLimit(const ResolutionRateLimit&) = default;
ResolutionRateLimit::ResolutionRateLimit(const gfx::Size& frame_size,
                                         int min_start_bitrate_bps,
                                         int min_bitrate_bps,
                                         int max_bitrate_bps,
                                         uint32_t max_framerate_numerator,
                                         uint32_t max_framerate_denominator)
    : frame_size(frame_size),
      min_start_bitrate_bps(min_start_bitrate_bps),
      min_bitrate_bps(min_bitrate_bps),
      max_bitrate_bps(max_bitrate_bps),
      max_framerate_numerator(max_framerate_numerator),
      max_framerate_denominator(max_framerate_denominator) {}
ResolutionRateLimit::~ResolutionRateLimit() = default;

bool operator==(const ResolutionRateLimit& lhs,
                const ResolutionRateLimit& rhs) {
  return std::tie(lhs.frame_size, lhs.min_start_bitrate_bps,
                  lhs.min_bitrate_bps, lhs.max_bitrate_bps,
                  lhs.max_framerate_numerator, lhs.max_framerate_denominator) ==
         std::tie(rhs.frame_size, rhs.min_start_bitrate_bps,
                  rhs.min_bitrate_bps, rhs.max_bitrate_bps,
                  rhs.max_framerate_numerator, rhs.max_framerate_denominator);
}

VideoEncoderInfo::VideoEncoderInfo() = default;
VideoEncoderInfo::VideoEncoderInfo(const VideoEncoderInfo&) = default;
VideoEncoderInfo::~VideoEncoderInfo() = default;

bool operator==(const VideoEncoderInfo& lhs, const VideoEncoderInfo& rhs) {
  return std::tie(lhs.implementation_name, lhs.frame_delay, lhs.input_capacity,
                  lhs.supports_native_handle, lhs.has_trusted_rate_controller,
                  lhs.is_hardware_accelerated, lhs.supports_simulcast,
                  lhs.reports_average_qp, lhs.requested_resolution_alignment,
                  lhs.apply_alignment_to_all_simulcast_layers,
                  lhs.fps_allocation, lhs.resolution_rate_limits,
                  lhs.supports_gpu_shared_images,
                  lhs.gpu_supported_pixel_formats) ==
         std::tie(rhs.implementation_name, rhs.frame_delay, rhs.input_capacity,
                  rhs.supports_native_handle, rhs.has_trusted_rate_controller,
                  rhs.is_hardware_accelerated, rhs.supports_simulcast,
                  rhs.reports_average_qp, rhs.requested_resolution_alignment,
                  rhs.apply_alignment_to_all_simulcast_layers,
                  rhs.fps_allocation, rhs.resolution_rate_limits,
                  rhs.supports_gpu_shared_images,
                  rhs.gpu_supported_pixel_formats);
}

bool VideoEncoderInfo::DoesSupportGpuSharedImages(
    gpu::SharedImageUsageSet usage,
    VideoPixelFormat format) const {
#if BUILDFLAG(IS_ANDROID)
  // Temporarily enforce VEA usage for shared images only on Android,
  // before we had a chance to audit shared image sources and types
  // on other platforms.
  if (!usage.Has(gpu::SHARED_IMAGE_USAGE_VIDEO_ENCODE_ACCELERATOR)) {
    return false;
  }
#endif

  if (!supports_gpu_shared_images) {
    return false;
  }

  return std::ranges::find(gpu_supported_pixel_formats, format) !=
         gpu_supported_pixel_formats.end();
}

}  // namespace media
