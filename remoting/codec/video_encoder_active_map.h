// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_ENCODER_ACTIVE_MAP_H_
#define REMOTING_CODEC_VIDEO_ENCODER_ACTIVE_MAP_H_

#include <cstddef>
#include <cstdint>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

class VideoEncoderActiveMap {
 public:
  VideoEncoderActiveMap();

  VideoEncoderActiveMap(const VideoEncoderActiveMap&) = delete;
  VideoEncoderActiveMap& operator=(const VideoEncoderActiveMap&) = delete;

  ~VideoEncoderActiveMap();

  // Initializes |active_map_| for an image of |size|.
  void Initialize(const webrtc::DesktopSize& size);

  // Clears |active_map_| without resetting the size.
  void Clear();

  // Marks all macroblocks in |active_map_| as active.
  void SetFull();

  // Updates the active map according to |updated_region|.
  void Update(const webrtc::DesktopRegion& updated_region);

  uint8_t* data() LIFETIME_BOUND { return active_map_.data(); }
  base::span<const uint8_t> as_span() const LIFETIME_BOUND {
    return active_map_.as_span();
  }

  uint32_t width() const { return active_map_size_.width(); }
  uint32_t height() const { return active_map_size_.height(); }
  size_t active_blocks() const { return active_blocks_; }
  size_t total_blocks() const { return active_map_.size(); }

 private:
  // Active map used to optimize out processing of unchanged macroblocks.
  base::HeapArray<uint8_t> active_map_;
  webrtc::DesktopSize active_map_size_;
  size_t active_blocks_ = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_ENCODER_ACTIVE_MAP_H_
