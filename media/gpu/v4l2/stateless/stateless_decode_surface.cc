// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/stateless_decode_surface.h"

#include "base/logging.h"
#include "media/gpu/macros.h"

namespace media {

StatelessDecodeSurface::StatelessDecodeSurface(uint32_t frame_id)
    : frame_id_(frame_id) {
  DVLOGF(4) << "Creating surface with id : " << frame_id_;
}

StatelessDecodeSurface::~StatelessDecodeSurface() {
  DVLOGF(4) << "Releasing surface with id : " << frame_id_;
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

}  // namespace media
