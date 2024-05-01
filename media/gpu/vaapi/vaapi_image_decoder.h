// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODER_H_
#define MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODER_H_

#include <stdint.h>
#include <va/va.h>

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "gpu/config/gpu_info.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace gfx {
class NativePixmapDmaBuf;
}  // namespace gfx

namespace media {

struct NativePixmapAndSizeInfo;
class ScopedVASurface;
class VaapiWrapper;

enum class VaapiFunctions;
using ReportErrorToUMACB = base::RepeatingCallback<void(VaapiFunctions)>;

struct VAContextAndScopedVASurfaceDeleter {
  void operator()(ScopedVASurface* scoped_va_surface) const;
};

using ScopedVAContextAndSurface =
    std::unique_ptr<ScopedVASurface, VAContextAndScopedVASurfaceDeleter>;

enum class VaapiImageDecodeStatus : uint32_t {
  kSuccess,
  kParseFailed,
  kUnsupportedImage,
  kUnsupportedSubsampling,
  kSurfaceCreationFailed,
  kSubmitVABuffersFailed,
  kExecuteDecodeFailed,
  kUnsupportedSurfaceFormat,
  kCannotGetImage,
  kCannotExportSurface,
  kInvalidState,
};

// This class abstracts the idea of VA-API format-specific decoders. It is the
// responsibility of each subclass to initialize |vaapi_wrapper_| appropriately
// for the purpose of performing hardware-accelerated image decodes of a
// particular format (e.g. JPEG or WebP). Objects of this class are not
// thread-safe, but they are also not thread-affine, i.e., the caller is free to
// call the methods on any thread, but calls must be synchronized externally.
class VaapiImageDecoder {
 public:
  VaapiImageDecoder(const VaapiImageDecoder&) = delete;
  VaapiImageDecoder& operator=(const VaapiImageDecoder&) = delete;

  virtual ~VaapiImageDecoder();

  // Initializes |vaapi_wrapper_| in kDecode mode with the appropriate VAAPI
  // profile and |error_uma_cb| for error reporting. When the VaapiImageDecoder
  // is already initialized, this is a no-op that returns true.
  virtual bool Initialize(const ReportErrorToUMACB& error_uma_cb);

  // Decodes a picture. It will fill VA-API parameters and call the
  // corresponding VA-API methods according to the image in |encoded_image|.
  // The image will be decoded into an internally allocated ScopedVASurface.
  // This VA surface will remain valid until the next Decode() call or
  // destruction of this class. Returns a VaapiImageDecodeStatus that will
  // indicate whether the decode succeeded or the reason it failed. Note that
  // the internal ScopedVASurface is destroyed on failure.
  virtual VaapiImageDecodeStatus Decode(
      base::span<const uint8_t> encoded_image);

  // Returns a pointer to the internally managed ScopedVASurface.
  virtual const ScopedVASurface* GetScopedVASurface() const;

  // Returns the type of image supported by this decoder.
  virtual gpu::ImageDecodeAcceleratorType GetType() const = 0;

  // Returns the type of mapping needed to convert the NativePixmapDmaBuf
  // returned by ExportAsNativePixmapDmaBuf() from YUV to RGB.
  virtual SkYUVColorSpace GetYUVColorSpace() const = 0;

  // Exports the decoded data from the last Decode() call as a
  // gfx::NativePixmapDmaBuf. Returns nullptr on failure and sets *|status| to
  // the reason for failure. On success, the image decoder gives up ownership of
  // the buffer underlying the NativePixmapDmaBuf.
  virtual std::unique_ptr<NativePixmapAndSizeInfo> ExportAsNativePixmapDmaBuf(
      VaapiImageDecodeStatus* status);

 protected:
  explicit VaapiImageDecoder(VAProfile va_profile);

  SEQUENCE_CHECKER(decoder_sequence_checker_);

  ScopedVAContextAndSurface scoped_va_context_and_surface_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  scoped_refptr<VaapiWrapper> vaapi_wrapper_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

 private:
  // Submits an image to the VA-API by filling its parameters and calling on the
  // corresponding methods according to the image in |encoded_image|. Returns a
  // VaapiImageDecodeStatus that will indicate whether the operation succeeded
  // or the reason it failed.
  virtual VaapiImageDecodeStatus AllocateVASurfaceAndSubmitVABuffers(
      base::span<const uint8_t> encoded_image) = 0;

  // The VA profile used for the current image decoder.
  const VAProfile va_profile_ GUARDED_BY_CONTEXT(decoder_sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODER_H_
