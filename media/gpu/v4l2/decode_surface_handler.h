// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_DECODE_SURFACE_HANDLER_H_
#define MEDIA_GPU_V4L2_DECODE_SURFACE_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

class VideoColorSpace;

// Interface representing {V4L2,Vaapi}DecodeSurface operations, i.e. a client
// gets a T to work with by calling CreateSurface() and returns it when finished
// by calling SurfaceReady(). Class T has to be ref-counted. No assumptions are
// made about threading.
template <class T>
class DecodeSurfaceHandler {
 public:
  DecodeSurfaceHandler() = default;

  DecodeSurfaceHandler(const DecodeSurfaceHandler&) = delete;
  DecodeSurfaceHandler& operator=(const DecodeSurfaceHandler&) = delete;

  virtual ~DecodeSurfaceHandler() = default;

  // Returns a T for decoding into, if available, or nullptr.
  virtual scoped_refptr<T> CreateSurface() = 0;

  // Called by the client to indicate that |dec_surface| is ready to be
  // outputted. This can actually be called before decode is finished in
  // hardware; this method must guarantee that |dec_surface|s are processed in
  // the same order as SurfaceReady() is called. (On Intel, this order doesn't
  // need to be explicitly maintained since the driver will enforce it, together
  // with any necessary dependencies).
  virtual void SurfaceReady(scoped_refptr<T> dec_surface,
                            int32_t bitstream_id,
                            const gfx::Rect& visible_rect,
                            const VideoColorSpace& color_space) = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_DECODE_SURFACE_HANDLER_H_
