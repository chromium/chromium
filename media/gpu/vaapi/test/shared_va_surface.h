// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_SHARED_VA_SURFACE_H_
#define MEDIA_GPU_VAAPI_TEST_SHARED_VA_SURFACE_H_

#include "base/memory/ref_counted.h"
#include "media/gpu/vaapi/test/vaapi_device.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace vaapi_test {

constexpr unsigned int kInvalidVaRtFormat = 0u;

// Provides a wrapper around a VASurface that properly handles creation and
// destruction.
// The associated VaapiDevice must be guaranteed externally to be alive beyond
// the lifetime of the SharedVASurface.
class SharedVASurface : public base::RefCounted<SharedVASurface> {
 public:
  // Constructs a VASurface with given |size| and |attribute|.
  static scoped_refptr<SharedVASurface> Create(const VaapiDevice& va_device,
                                               unsigned int va_rt_format,
                                               const gfx::Size& size,
                                               VASurfaceAttrib attribute);

  // Saves this surface into a png at the given |path|. The image data is
  // retrieved by first attempting to call vaDeriveImage on the surface;
  // if that fails or returns an unsupported format, fall back to
  // vaCreateImage + vaGetImage with NV12 or P010 as appropriate.
  // NB: vaDeriveImage may succeed but fetch garbage output in AMD.
  void SaveAsPNG(const std::string& path);

  // Computes the MD5 sum of this surface and returns it as a human-readable hex
  // string.
  std::string GetMD5Sum() const;

  VASurfaceID id() const { return id_; }
  const gfx::Size& size() const { return size_; }
  unsigned int va_rt_format() const { return va_rt_format_; }

 private:
  friend class base::RefCounted<SharedVASurface>;

  SharedVASurface(const VaapiDevice& va_device,
                  VASurfaceID id,
                  const gfx::Size& size,
                  unsigned int format);

  SharedVASurface(const SharedVASurface&) = delete;
  SharedVASurface& operator=(const SharedVASurface&) = delete;
  ~SharedVASurface();

  // Non-owned.
  const VaapiDevice& va_device_;

  const VASurfaceID id_;
  const gfx::Size size_;
  const unsigned int va_rt_format_;
};

}  // namespace vaapi_test
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_SHARED_VA_SURFACE_H_
