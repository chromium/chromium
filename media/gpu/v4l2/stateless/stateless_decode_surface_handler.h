// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_STATELESS_DECODE_SURFACE_HANDLER_H_
#define MEDIA_GPU_V4L2_STATELESS_STATELESS_DECODE_SURFACE_HANDLER_H_

#include "media/gpu/decode_surface_handler.h"
#include "media/gpu/v4l2/v4l2_decode_surface.h"

namespace media {

class StatelessDecodeSurfaceHandler
    : public DecodeSurfaceHandler<V4L2DecodeSurface> {
 public:
  StatelessDecodeSurfaceHandler() = default;

  StatelessDecodeSurfaceHandler(const StatelessDecodeSurfaceHandler&) = delete;
  StatelessDecodeSurfaceHandler& operator=(
      const StatelessDecodeSurfaceHandler&) = delete;

  ~StatelessDecodeSurfaceHandler() override = default;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_STATELESS_DECODE_SURFACE_HANDLER_H_
