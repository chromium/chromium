// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_ENCODER_ACTIVE_MAP_H_
#define REMOTING_CODEC_VIDEO_ENCODER_ACTIVE_MAP_H_

#include <memory>

#include <stdint.h>

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

  // Updates the active map according to |updated_region|.
  void Update(const webrtc::DesktopRegion& updated_region);

  uint8_t* data() { return active_map_.get(); }

  uint32_t width() { return active_map_size_.width(); }
  uint32_t height() { return active_map_size_.height(); }

 private:
  // Active map used to optimize out processing of unchanged macroblocks.
  std::unique_ptr<uint8_t[]> active_map_;
  webrtc::DesktopSize active_map_size_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_ENCODER_ACTIVE_MAP_H_
