// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_active_map.h"

#include <algorithm>
#include <utility>

#include "base/containers/heap_array.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

using DesktopRegionIterator = webrtc::DesktopRegion::Iterator;

namespace {
// Defines the dimension of a macro block. This is used to compute the active
// map for an encoder. This size is the same for VP8, VP9, and AV1 encoders.
constexpr int kMacroBlockSize = 16;
}  // namespace

namespace remoting {

VideoEncoderActiveMap::VideoEncoderActiveMap() = default;

VideoEncoderActiveMap::~VideoEncoderActiveMap() = default;

void VideoEncoderActiveMap::Initialize(const webrtc::DesktopSize& size) {
  active_map_size_.set((size.width() + kMacroBlockSize - 1) / kMacroBlockSize,
                       (size.height() + kMacroBlockSize - 1) / kMacroBlockSize);
  active_map_ = base::HeapArray<uint8_t>::Uninit(active_map_size_.width() *
                                                 active_map_size_.height());
  Clear();
}

void VideoEncoderActiveMap::Clear() {
  std::ranges::fill(active_map_, 0);
}

void VideoEncoderActiveMap::Update(
    const webrtc::DesktopRegion& updated_region) {
  if (active_map_.empty()) {
    return;
  }
  const int map_width = active_map_size_.width();
  const int map_height = active_map_size_.height();
  for (DesktopRegionIterator r(updated_region); !r.IsAtEnd(); r.Advance()) {
    const webrtc::DesktopRect& rect = r.rect();
    const int left =
        std::clamp(rect.left() / kMacroBlockSize, 0, map_width - 1);
    const int right =
        std::clamp((rect.right() - 1) / kMacroBlockSize, 0, map_width - 1);
    const int top = std::clamp(rect.top() / kMacroBlockSize, 0, map_height - 1);
    const int bottom =
        std::clamp((rect.bottom() - 1) / kMacroBlockSize, 0, map_height - 1);
    for (int y = top; y <= bottom; ++y) {
      const size_t row_offset = static_cast<size_t>(y) * map_width;
      for (int x = left; x <= right; ++x) {
        active_map_[row_offset + x] = 1;
      }
    }
  }
}

}  // namespace remoting
