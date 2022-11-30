// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_DECODE_SURFACE_HANDLER_H_
#define MEDIA_GPU_V4L2_V4L2_DECODE_SURFACE_HANDLER_H_

#include <linux/videodev2.h>

#include "media/gpu/decode_surface_handler.h"
#include "media/gpu/v4l2/v4l2_decode_surface.h"

namespace media {

class V4L2DecodeSurfaceHandler
    : public DecodeSurfaceHandler<V4L2DecodeSurface> {
 public:
  V4L2DecodeSurfaceHandler() = default;

  V4L2DecodeSurfaceHandler(const V4L2DecodeSurfaceHandler&) = delete;
  V4L2DecodeSurfaceHandler& operator=(const V4L2DecodeSurfaceHandler&) = delete;

  ~V4L2DecodeSurfaceHandler() override = default;

  // Append slice data in |data| of size |size| to pending hardware
  // input buffer with |index|. This buffer will be submitted for decode
  // on the next DecodeSurface(). Return true on success.
  virtual bool SubmitSlice(V4L2DecodeSurface* dec_surface,
                           const uint8_t* data,
                           size_t size) = 0;

  // Decode the surface |dec_surface|.
  virtual void DecodeSurface(scoped_refptr<V4L2DecodeSurface> dec_surface) = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_DECODE_SURFACE_HANDLER_H_
