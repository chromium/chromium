// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_DECODE_SURFACE_HANDLER_H_
#define MEDIA_GPU_VAAPI_VAAPI_DECODE_SURFACE_HANDLER_H_

#include "media/gpu/vaapi/vaapi_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

class VASurface;
class VideoColorSpace;

using VASurfaceID = unsigned int;

// Interface representing Vaapi DecodeSurface operations, i.e. a client gets a
// VASurface to work with by calling CreateSurface() and returns it when
// finished by calling SurfaceReady(). No assumptions are made about
// threading.
class VaapiDecodeSurfaceHandler {
 public:
  VaapiDecodeSurfaceHandler() = default;

  VaapiDecodeSurfaceHandler(const VaapiDecodeSurfaceHandler&) = delete;
  VaapiDecodeSurfaceHandler& operator=(const VaapiDecodeSurfaceHandler&) =
      delete;

  virtual ~VaapiDecodeSurfaceHandler() = default;

  // Returns a VASurface for decoding into, if available, or nullptr.
  virtual std::unique_ptr<VASurfaceHandle> CreateSurface() = 0;

  // Called by the client to indicate that |va_surface_id| is ready to be
  // outputted. This can actually be called before decode is finished in
  // hardware; this method must guarantee that |dec_surface|s are processed in
  // the same order as SurfaceReady() is called. (On Intel, this order doesn't
  // need to be explicitly maintained since the driver will enforce it, together
  // with any necessary dependencies).
  virtual void SurfaceReady(VASurfaceID va_surface_id,
                            int32_t bitstream_id,
                            const gfx::Rect& visible_rect,
                            const VideoColorSpace& color_space) = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_DECODE_SURFACE_HANDLER_H_
