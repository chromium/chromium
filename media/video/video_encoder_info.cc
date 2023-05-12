// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encoder_info.h"

#include <tuple>

namespace media {

ResolutionBitrateLimit::ResolutionBitrateLimit() = default;
ResolutionBitrateLimit::ResolutionBitrateLimit(const ResolutionBitrateLimit&) =
    default;
ResolutionBitrateLimit::ResolutionBitrateLimit(const gfx::Size& frame_size,
                                               int min_start_bitrate_bps,
                                               int min_bitrate_bps,
                                               int max_bitrate_bps)
    : frame_size(frame_size),
      min_start_bitrate_bps(min_start_bitrate_bps),
      min_bitrate_bps(min_bitrate_bps),
      max_bitrate_bps(max_bitrate_bps) {}
ResolutionBitrateLimit::~ResolutionBitrateLimit() = default;

bool operator==(const ResolutionBitrateLimit& lhs,
                const ResolutionBitrateLimit& rhs) {
  return std::tie(lhs.frame_size, lhs.min_start_bitrate_bps,
                  lhs.min_bitrate_bps, lhs.max_bitrate_bps) ==
         std::tie(rhs.frame_size, rhs.min_start_bitrate_bps,
                  rhs.min_bitrate_bps, rhs.max_bitrate_bps);
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
                  lhs.fps_allocation, lhs.resolution_bitrate_limits) ==
         std::tie(rhs.implementation_name, rhs.frame_delay, rhs.input_capacity,
                  rhs.supports_native_handle, rhs.has_trusted_rate_controller,
                  rhs.is_hardware_accelerated, rhs.supports_simulcast,
                  rhs.reports_average_qp, rhs.requested_resolution_alignment,
                  rhs.apply_alignment_to_all_simulcast_layers,
                  rhs.fps_allocation, rhs.resolution_bitrate_limits);
}

}  // namespace media
