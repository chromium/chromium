// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_UTILS_H_
#define MEDIA_GPU_VAAPI_VAAPI_UTILS_H_

#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/thread_annotations.h"
#include "ui/gfx/geometry/size.h"

// Forward declarations taken verbatim from <va/va.h>
typedef unsigned int VABufferID;
typedef void* VADisplay;
typedef struct _VAImage VAImage;
typedef struct _VAImageFormat VAImageFormat;
typedef int VAStatus;
typedef unsigned int VASurfaceID;

namespace base {
class Lock;
}

namespace media {
class VaapiWrapper;
class Vp8ReferenceFrameVector;
struct VAContextAndScopedVASurfaceDeleter;
struct Vp8FrameHeader;

constexpr uint32_t kInvalidVaFourcc = 0u;

// Class to map a given VABuffer, identified by |buffer_id|, for its lifetime.
// This class must operate under |lock_| acquired.
class ScopedVABufferMapping {
 public:
  // |release_callback| will be called if the mapping of the buffer failed.
  ScopedVABufferMapping(const base::Lock* lock,
                        VADisplay va_display,
                        VABufferID buffer_id,
                        base::OnceCallback<void(VABufferID)> release_callback =
                            base::NullCallback());
  ~ScopedVABufferMapping();
  bool IsValid() const { return !!va_buffer_data_; }
  void* data() const {
    DCHECK(IsValid());
    return va_buffer_data_;
  }
  // Explicit destruction method, to retrieve the success/error result. It is
  // safe to call this method several times.
  VAStatus Unmap();

 private:
  const base::Lock* lock_;  // Only for AssertAcquired() calls.
  const VADisplay va_display_;
  const VABufferID buffer_id_;

  void* va_buffer_data_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScopedVABufferMapping);
};

// This class tracks the VAImage life cycle from vaCreateImage() - vaGetImage()
// to vaDestroyImage(). In between creation and destruction, image()->buf  will
// try to be be mapped on user space using a ScopedVABufferMapping. All
// resources will be cleaned up appropriately. |lock| is acquired for
// destruction purposes.
class ScopedVAImage {
 public:
  ScopedVAImage(base::Lock* lock,
                VADisplay va_display,
                VASurfaceID va_surface_id,
                VAImageFormat* format /* Needs to be a pointer for libva */,
                const gfx::Size& size);
  ~ScopedVAImage();

  bool IsValid() const { return va_buffer_ && va_buffer_->IsValid(); }

  const VAImage* image() const { return image_.get(); }
  const ScopedVABufferMapping* va_buffer() const {
    DCHECK(IsValid());
    return va_buffer_.get();
  }

 private:
  base::Lock* lock_;
  const VADisplay va_display_ GUARDED_BY(lock_);
  std::unique_ptr<VAImage> image_;
  std::unique_ptr<ScopedVABufferMapping> va_buffer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedVAImage);
};

// A VA-API-specific surface used by video/image codec accelerators to work on.
// As the name suggests, this class is self-cleaning.
class ScopedVASurface {
 public:
  ScopedVASurface(scoped_refptr<VaapiWrapper> vaapi_wrapper,
                  VASurfaceID va_surface_id,
                  const gfx::Size& size,
                  unsigned int va_rt_format);
  ~ScopedVASurface();

  bool IsValid() const;
  VASurfaceID id() const { return va_surface_id_; }
  const gfx::Size& size() const { return size_; }
  unsigned int format() const { return va_rt_format_; }

 private:
  friend struct VAContextAndScopedVASurfaceDeleter;
  const scoped_refptr<VaapiWrapper> vaapi_wrapper_;
  const VASurfaceID va_surface_id_;
  const gfx::Size size_;
  const unsigned int va_rt_format_;

  DISALLOW_COPY_AND_ASSIGN(ScopedVASurface);
};

// Adapts |frame_header| to the Vaapi data types, prepping it for consumption by
// |vaapi_wrapper|
bool FillVP8DataStructures(const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
                           VASurfaceID va_surface_id,
                           const Vp8FrameHeader& frame_header,
                           const Vp8ReferenceFrameVector& reference_frames);
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_UTILS_H_
