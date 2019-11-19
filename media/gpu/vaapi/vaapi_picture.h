// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an interface of output pictures for the Vaapi
// video decoder. This is implemented by different window system
// (X11/Ozone) and used by VaapiVideoDecodeAccelerator to produce
// output pictures.

#ifndef MEDIA_GPU_VAAPI_VAAPI_PICTURE_H_
#define MEDIA_GPU_VAAPI_VAAPI_PICTURE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

using VASurfaceID = unsigned int;

class VASurface;
class VaapiWrapper;

// Picture is native pixmap abstraction (X11/Ozone).
class MEDIA_GPU_EXPORT VaapiPicture {
 public:
  virtual ~VaapiPicture();

  // Uses the buffer of |format|, pointed to by |gpu_memory_buffer_handle| as
  // the backing storage for this picture. This takes ownership of the handle
  // and will close it even on failure. Return true on success, false otherwise.
  virtual bool ImportGpuMemoryBufferHandle(
      gfx::BufferFormat format,
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) = 0;

  // Allocates a buffer of |format| to use as backing storage for this picture.
  // Return true on success.
  virtual bool Allocate(gfx::BufferFormat format) = 0;

  int32_t picture_buffer_id() const { return picture_buffer_id_; }

  virtual bool AllowOverlay() const;

  // Downloads |va_surface| into the picture, potentially scaling it if needed.
  virtual bool DownloadFromSurface(
      const scoped_refptr<VASurface>& va_surface) = 0;

  // Returns the associated VASurfaceID, if any, or VA_INVALID_ID.
  virtual VASurfaceID va_surface_id() const;

 protected:
  VaapiPicture(const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
               const MakeGLContextCurrentCallback& make_context_current_cb,
               const BindGLImageCallback& bind_image_cb,
               int32_t picture_buffer_id,
               const gfx::Size& size,
               uint32_t texture_id,
               uint32_t client_texture_id,
               uint32_t texture_target);

  const scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  const MakeGLContextCurrentCallback make_context_current_cb_;
  const BindGLImageCallback bind_image_cb_;

  const gfx::Size size_;
  const uint32_t texture_id_;
  const uint32_t client_texture_id_;
  const uint32_t texture_target_;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  const int32_t picture_buffer_id_;

  DISALLOW_COPY_AND_ASSIGN(VaapiPicture);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_PICTURE_H_
