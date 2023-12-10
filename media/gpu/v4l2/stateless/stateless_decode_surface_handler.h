// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_STATELESS_DECODE_SURFACE_HANDLER_H_
#define MEDIA_GPU_V4L2_STATELESS_STATELESS_DECODE_SURFACE_HANDLER_H_

#include "media/gpu/decode_surface_handler.h"
#include "media/gpu/v4l2/stateless/stateless_decode_surface.h"

namespace media {

class StatelessDecodeSurfaceHandler
    : public DecodeSurfaceHandler<StatelessDecodeSurface> {
 public:
  StatelessDecodeSurfaceHandler() = default;

  StatelessDecodeSurfaceHandler(const StatelessDecodeSurfaceHandler&) = delete;
  StatelessDecodeSurfaceHandler& operator=(
      const StatelessDecodeSurfaceHandler&) = delete;

  ~StatelessDecodeSurfaceHandler() override = default;

  // The stateless api requires that the client parse the header information
  // and send that separately. |ctrls| is the parsed header while |data| is
  // the complete frame of compressed data with |size| being the length of
  // the buffer. |frame_id| is an identifier for the driver to use for
  // reference frame designation.
  virtual bool SubmitFrame(void* ctrls,
                           const uint8_t* data,
                           size_t size,
                           uint32_t frame_id) = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_STATELESS_DECODE_SURFACE_HANDLER_H_
