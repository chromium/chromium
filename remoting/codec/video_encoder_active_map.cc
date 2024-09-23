// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/codec/video_encoder_active_map.h"

#include <algorithm>
#include <utility>

#include <string.h>

#include "base/check_op.h"
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
  active_map_ = std::make_unique<uint8_t[]>(active_map_size_.width() *
                                            active_map_size_.height());
  Clear();
}

void VideoEncoderActiveMap::Clear() {
  DCHECK(active_map_);
  memset(active_map_.get(), 0,
         active_map_size_.width() * active_map_size_.height());
}

void VideoEncoderActiveMap::Update(
    const webrtc::DesktopRegion& updated_region) {
  for (DesktopRegionIterator r(updated_region); !r.IsAtEnd(); r.Advance()) {
    const webrtc::DesktopRect& rect = r.rect();
    int left = rect.left() / kMacroBlockSize;
    int right = (rect.right() - 1) / kMacroBlockSize;
    int top = rect.top() / kMacroBlockSize;
    int bottom = (rect.bottom() - 1) / kMacroBlockSize;
    DCHECK_LT(right, active_map_size_.width());
    DCHECK_LT(bottom, active_map_size_.height());

    uint8_t* map = active_map_.get() + top * active_map_size_.width();
    for (int y = top; y <= bottom; ++y) {
      for (int x = left; x <= right; ++x) {
        map[x] = 1;
      }
      map += active_map_size_.width();
    }
  }
}

}  // namespace remoting
