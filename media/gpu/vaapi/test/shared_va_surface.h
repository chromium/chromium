// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_SHARED_VA_SURFACE_H_
#define MEDIA_GPU_VAAPI_TEST_SHARED_VA_SURFACE_H_

#include <va/va.h>

#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/types/pass_key.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace vaapi_test {

class VaapiDevice;

constexpr unsigned int kInvalidVaRtFormat = 0u;

// Provides a wrapper around a VASurface that properly handles creation and
// destruction.
// The associated VaapiDevice must be guaranteed externally to be alive beyond
// the lifetime of the SharedVASurface.
class SharedVASurface : public base::RefCounted<SharedVASurface> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  // How to fetch image data from a SharedVASurface.
  enum class FetchPolicy {
    // Fetch the data by attempting all policies in the order listed below.
    kAny,

    // Use vaDeriveImage. Deriving to unsupported formats results in failure.
    // NB: vaDeriveImage may succeed but fetch garbage output in AMD.
    kDeriveImage,

    // Use vaCreateImage + vaGetImage. Requires a target format.
    kGetImage,
  };

  // Constructs a VASurface with given |size| and |attribute|.
  static scoped_refptr<SharedVASurface> Create(const VaapiDevice& va_device,
                                               unsigned int va_rt_format,
                                               const gfx::Size& size,
                                               VASurfaceAttrib attribute);

  SharedVASurface(base::PassKey<SharedVASurface>,
                  const VaapiDevice& va_device,
                  VASurfaceID id,
                  const gfx::Size& size,
                  unsigned int format);

  // Saves this surface into a png at the given |path|, retrieving the image
  // as specified by |fetch_policy|.
  void SaveAsPNG(FetchPolicy fetch_policy, const std::string& path) const;

  // Computes the MD5 sum of this surface and returns it as a human-readable hex
  // string.
  std::string GetMD5Sum(FetchPolicy fetch_policy) const;

  VASurfaceID id() const { return id_; }
  const gfx::Size& size() const { return size_; }
  unsigned int va_rt_format() const { return va_rt_format_; }

 private:
  friend class base::RefCounted<SharedVASurface>;
  ~SharedVASurface();

  SharedVASurface(const SharedVASurface&) = delete;
  SharedVASurface& operator=(const SharedVASurface&) = delete;

  // Fetch the image data from this SharedVASurface with |fetch_policy|.
  // |format| may be ignored if |fetch_policy| specifies derivation which
  // succeeds to a supported format.
  void FetchData(FetchPolicy fetch_policy,
                 const VAImageFormat& format,
                 VAImage* image,
                 uint8_t** image_data) const;

  // Non-owned.
  const raw_ref<const VaapiDevice> va_device_;

  const VASurfaceID id_;
  const gfx::Size size_;
  const unsigned int va_rt_format_;
};

}  // namespace vaapi_test
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_SHARED_VA_SURFACE_H_
