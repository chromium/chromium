// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/stateless_decode_surface.h"

namespace media {

StatelessDecodeSurface::StatelessDecodeSurface(uint32_t frame_id)
    : frame_id_(frame_id) {}

StatelessDecodeSurface::~StatelessDecodeSurface() {}

}  // namespace media
