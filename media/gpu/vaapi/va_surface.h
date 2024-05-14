// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the definition of VASurface class, used for decoding by
// VaapiVideoDecodeAccelerator and VaapiH264Decoder.

#ifndef MEDIA_GPU_VAAPI_VA_SURFACE_H_
#define MEDIA_GPU_VAAPI_VA_SURFACE_H_

#include <va/va.h>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// A VA-API-specific VASurfaceID and metadata holder.
// TODO(339518553): Remove this class in favour of its not ref-counted
// ScopedVASurface sibling.
class VASurface : public base::RefCountedThreadSafe<VASurface> {
 public:
  using ReleaseCB = base::OnceCallback<void(VASurfaceID)>;

  VASurface(VASurfaceID va_surface_id,
            const gfx::Size& size,
            unsigned int format,
            ReleaseCB release_cb);

  VASurface(const VASurface&) = delete;
  VASurface& operator=(const VASurface&) = delete;

  VASurfaceID id() const { return va_surface_id_; }
  const gfx::Size& size() const { return size_; }
  unsigned int format() const { return format_; }

 private:
  friend class base::RefCountedThreadSafe<VASurface>;
  ~VASurface();

  const VASurfaceID va_surface_id_;
  const gfx::Size size_;
  const unsigned int format_;
  ReleaseCB release_cb_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VA_SURFACE_H_
