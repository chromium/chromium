// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encoder_info.h"

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

VideoEncoderInfo::VideoEncoderInfo() = default;
VideoEncoderInfo::VideoEncoderInfo(const VideoEncoderInfo&) = default;
VideoEncoderInfo::~VideoEncoderInfo() = default;

bool operator==(const ResolutionBitrateLimit& l,
                const ResolutionBitrateLimit& r) {
  return l.frame_size == r.frame_size &&
         l.min_start_bitrate_bps == r.min_start_bitrate_bps &&
         l.min_bitrate_bps == r.min_bitrate_bps &&
         l.max_bitrate_bps == r.max_bitrate_bps;
}

bool operator==(const VideoEncoderInfo& l, const VideoEncoderInfo& r) {
  for (size_t i = 0; i < VideoEncoderInfo::kMaxSpatialLayers; ++i) {
    if (l.fps_allocation[i] != r.fps_allocation[i])
      return false;
  }

  return l.implementation_name == r.implementation_name &&
         l.supports_native_handle == r.supports_native_handle &&
         l.has_trusted_rate_controller == r.has_trusted_rate_controller &&
         l.is_hardware_accelerated == r.is_hardware_accelerated &&
         l.supports_simulcast == r.supports_simulcast &&
         l.reports_average_qp == r.reports_average_qp &&
         l.requested_resolution_alignment == r.requested_resolution_alignment &&
         l.apply_alignment_to_all_simulcast_layers ==
             r.apply_alignment_to_all_simulcast_layers &&
         l.resolution_bitrate_limits == r.resolution_bitrate_limits;
}
}  // namespace media
