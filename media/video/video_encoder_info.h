// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_ENCODER_INFO_H_
#define MEDIA_VIDEO_VIDEO_ENCODER_INFO_H_

#include <stdint.h>

#include <array>
#include <optional>
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

MEDIA_EXPORT bool operator==(const ResolutionBitrateLimit& lhs,
                             const ResolutionBitrateLimit& rhs);

struct MEDIA_EXPORT VideoEncoderInfo {
  static constexpr size_t kMaxSpatialLayers = 5;

  VideoEncoderInfo();
  VideoEncoderInfo(const VideoEncoderInfo&);
  ~VideoEncoderInfo();

  std::string implementation_name;

  // The number of additional input frames that must be enqueued before the
  // encoder starts producing output for the first frame, i.e., the size of the
  // compression window. Equal to 0 if the encoder can produce a chunk of
  // output just from the frame submitted last.
  // If absent, the encoder client will assume some default value.
  std::optional<int> frame_delay;

  // The number of input frames the encoder can queue internally. Once this
  // number is reached, further encode requests can block until some output has
  // been produced.
  // If absent, the encoder client will assume some default value.
  std::optional<int> input_capacity;

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
  // True if encoder supports frame size change without re-initialization.
  bool supports_frame_size_change = false;

  // Number of video encoder buffers (encoded frame slots) that
  // are kept by the encoder and can be used to store encoded frames for
  // future reference.
  size_t number_of_manual_reference_buffers = 0;

  std::array<std::vector<uint8_t>, kMaxSpatialLayers> fps_allocation;
  std::vector<ResolutionBitrateLimit> resolution_bitrate_limits;
};

MEDIA_EXPORT bool operator==(const VideoEncoderInfo& lhs,
                             const VideoEncoderInfo& rhs);

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_ENCODER_INFO_H_
