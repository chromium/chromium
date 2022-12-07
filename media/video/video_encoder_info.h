// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_ENCODER_INFO_H_
#define MEDIA_VIDEO_VIDEO_ENCODER_INFO_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "media/base/media_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// These chromium classes are the corresponding classes in webrtc project.
// See third_party/webrtc/api/video_codecs/video_encoder.h for the detail.

struct MEDIA_EXPORT ResolutionBitrateLimit {
  ResolutionBitrateLimit();
  ResolutionBitrateLimit(const ResolutionBitrateLimit&);
  ResolutionBitrateLimit(const gfx::Size& frame_size,
                         int min_start_bitrate_bps,
                         int min_bitrate_bps,
                         int max_bitrate_bps);
  ~ResolutionBitrateLimit();

  gfx::Size frame_size;
  int min_start_bitrate_bps = 0;
  int min_bitrate_bps = 0;
  int max_bitrate_bps = 0;
};

struct MEDIA_EXPORT VideoEncoderInfo {
  static constexpr size_t kMaxSpatialLayers = 5;

  VideoEncoderInfo();
  VideoEncoderInfo(const VideoEncoderInfo&);
  ~VideoEncoderInfo();

  std::string implementation_name;

  bool supports_native_handle = true;
  bool has_trusted_rate_controller = false;
  bool is_hardware_accelerated = true;
  bool supports_simulcast = false;
  // True if encoder uses same QP for all macroblocks of a picture without
  // per-macroblock QP adjustment, and that QP can be calculated from
  // uncompressed sequence/frame/slice/tile headers.
  bool reports_average_qp = true;
  uint32_t requested_resolution_alignment = 1;
  bool apply_alignment_to_all_simulcast_layers = false;

  std::vector<uint8_t> fps_allocation[kMaxSpatialLayers];
  std::vector<ResolutionBitrateLimit> resolution_bitrate_limits;
};

MEDIA_EXPORT bool operator==(const ResolutionBitrateLimit& l,
                             const ResolutionBitrateLimit& r);
MEDIA_EXPORT bool operator==(const VideoEncoderInfo& l,
                             const VideoEncoderInfo& r);
}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_ENCODER_INFO_H_
