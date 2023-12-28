// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/stateless_decode_surface.h"

#include "base/logging.h"
#include "media/gpu/macros.h"

namespace media {

StatelessDecodeSurface::StatelessDecodeSurface(uint64_t frame_id,
                                               base::OnceClosure enqueue_cb)
    : frame_id_(frame_id), enqueue_cb_(std::move(enqueue_cb)) {
  DVLOGF(3) << "Creating surface with id  : " << frame_id_;
}

StatelessDecodeSurface::~StatelessDecodeSurface() {
  DVLOGF(3) << "Releasing surface with id : " << frame_id_;
  std::move(enqueue_cb_).Run();
}

void StatelessDecodeSurface::SetVisibleRect(const gfx::Rect& visible_rect) {
  visible_rect_ = visible_rect;
}

void StatelessDecodeSurface::SetColorSpace(const VideoColorSpace& color_space) {
  color_space_ = color_space;
}

void StatelessDecodeSurface::SetVideoFrameTimestamp(
    const base::TimeDelta timestamp) {
  video_frame_timestamp_ = timestamp;
}

uint64_t StatelessDecodeSurface::GetReferenceTimestamp() const {
  const uint32_t kMicrosecondsToNanoseconds = 1000;
  return frame_id_ * kMicrosecondsToNanoseconds;
}

void StatelessDecodeSurface::SetReferenceSurfaces(
    std::vector<scoped_refptr<StatelessDecodeSurface>> ref_surfaces) {
  DCHECK(reference_surfaces_.empty());

  reference_surfaces_ = std::move(ref_surfaces);
}

void StatelessDecodeSurface::ClearReferenceSurfaces() {
  reference_surfaces_.clear();
}

}  // namespace media
