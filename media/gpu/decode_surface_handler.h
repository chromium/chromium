// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_DECODE_SURFACE_HANDLER_H_
#define MEDIA_GPU_DECODE_SURFACE_HANDLER_H_

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
  virtual ~DecodeSurfaceHandler() = default;

  // Returns a T for decoding into and for output, if available, or nullptr.
  virtual scoped_refptr<T> CreateSurface() = 0;

  // Used by implementations that scale the video between decode and output. In
  // those cases, the CreateSurface() call will be used for allocating the
  // output surfaces and CreateDecodeSurface() will be used for decoding
  // surfaces. This mode can be detected by calling IsScalingDecode().
  virtual scoped_refptr<T> CreateDecodeSurface() { return nullptr; }

  // Returns true if there are separate surfaces for decoding and output due to
  // a scaling operation being performed between the two.
  virtual bool IsScalingDecode() { return false; }

  // Returns the visible rect relative to the output surface if we are in
  // scaling mode. The |decode_visible_rect| should be passed in as well as the
  // |output_picture_size| for validation. The returned rect will only differ if
  // IsScalingDecode() is true.
  virtual const gfx::Rect GetOutputVisibleRect(
      const gfx::Rect& decode_visible_rect,
      const gfx::Size& output_picture_size) {
    CHECK(gfx::Rect(output_picture_size).Contains(decode_visible_rect));
    return decode_visible_rect;
  }

  // Called by the client to indicate that |dec_surface| is ready to be
  // outputted. |dec_surface| must be obtained from CreateSurface() and NOT from
  // CreateDecodeSurface(). This can actually be called before decode is
  // finished in hardware; this method must guarantee that |dec_surface|s are
  // processed in the same order as SurfaceReady() is called. (On Intel, this
  // order doesn't need to be explicitly maintained since the driver will
  // enforce it, together with any necessary dependencies).
  virtual void SurfaceReady(scoped_refptr<T> dec_surface,
                            int32_t bitstream_id,
                            const gfx::Rect& visible_rect,
                            const VideoColorSpace& color_space) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecodeSurfaceHandler);
};

}  // namespace media

#endif  // MEDIA_GPU_DECODE_SURFACE_HANDLER_H_
