// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_STATIC_BITMAP_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_STATIC_BITMAP_IMAGE_H_

#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace blink {

class PLATFORM_EXPORT StaticBitmapImage : public Image {
 public:
  // The ImageOrientation should be derived from the source of the image data.
  static scoped_refptr<StaticBitmapImage> Create(
      PaintImage,
      ImageOrientation = ImageOrientationEnum::kDefault);
  static scoped_refptr<StaticBitmapImage> Create(
      sk_sp<SkData> data,
      const SkImageInfo&,
      ImageOrientation = ImageOrientationEnum::kDefault);

  StaticBitmapImage(ImageOrientation orientation) : orientation_(orientation) {}

  bool IsStaticBitmapImage() const override { return true; }

  // Methods overridden by all sub-classes
  ~StaticBitmapImage() override = default;

  IntSize PreferredDisplaySize() const override;

  virtual scoped_refptr<StaticBitmapImage> ConvertToColorSpace(
      sk_sp<SkColorSpace>,
      SkColorType = kN32_SkColorType) = 0;

  // Methods have common implementation for all sub-classes
  bool CurrentFrameIsComplete() override { return true; }
  void DestroyDecodedData() override {}

  // Methods that have a default implementation, and overridden by only one
  // sub-class
  virtual bool IsValid() const { return true; }
  virtual void Transfer() {}
  virtual bool IsOriginTopLeft() const { return true; }

  // Creates a non-gpu copy of the image, or returns this if image is already
  // non-gpu.
  virtual scoped_refptr<StaticBitmapImage> MakeUnaccelerated() { return this; }

  // Methods overridden by AcceleratedStaticBitmapImage only
  // Assumes the destination texture has already been allocated.
  virtual bool CopyToTexture(gpu::gles2::GLES2Interface*,
                             GLenum,
                             GLuint,
                             GLint,
                             bool,
                             bool,
                             const IntPoint&,
                             const IntRect&) {
    NOTREACHED();
    return false;
  }

  virtual void EnsureSyncTokenVerified() { NOTREACHED(); }
  virtual gpu::MailboxHolder GetMailboxHolder() const {
    NOTREACHED();
    return gpu::MailboxHolder();
  }
  virtual void UpdateSyncToken(const gpu::SyncToken&) { NOTREACHED(); }
  virtual bool IsPremultiplied() const { return true; }

  // Methods have exactly the same implementation for all sub-classes
  bool OriginClean() const { return is_origin_clean_; }
  void SetOriginClean(bool flag) { is_origin_clean_ = flag; }

  // StaticBitmapImage needs to store the orientation of the image itself,
  // because the underlying representations do not. If the bitmap represents
  // a non-default orientation it must be explicitly given in the constructor.
  ImageOrientation CurrentFrameOrientation() const override {
    return orientation_;
  }
  bool HasDefaultOrientation() const override {
    return orientation_ == ImageOrientationEnum::kDefault;
  }

  static base::CheckedNumeric<size_t> GetSizeInBytes(
      const IntRect& rect,
      const CanvasColorParams& color_params);

  static bool MayHaveStrayArea(scoped_refptr<StaticBitmapImage> src_image,
                               const IntRect& rect);

  static bool CopyToByteArray(scoped_refptr<StaticBitmapImage> src_image,
                              base::span<uint8_t> dst,
                              const IntRect&,
                              const CanvasColorParams&);

 protected:
  // Helper for sub-classes
  void DrawHelper(cc::PaintCanvas*,
                  const cc::PaintFlags&,
                  const FloatRect&,
                  const FloatRect&,
                  ImageClampingMode,
                  RespectImageOrientationEnum,
                  const PaintImage&);

  // The image orientation is stored here because it is only available when the
  // static image is created and the underlying representations do not store
  // the information. The property is set at construction based on the source of
  // the image data.
  ImageOrientation orientation_ = ImageOrientationEnum::kDefault;

  // The following property is here because the SkImage API doesn't expose the
  // info. It is applied to both UnacceleratedStaticBitmapImage and
  // AcceleratedStaticBitmapImage. To change this property, the call site would
  // have to call SetOriginClean().
  bool is_origin_clean_ = true;
};

template <>
struct DowncastTraits<StaticBitmapImage> {
  static bool AllowFrom(const Image& image) {
    return image.IsStaticBitmapImage();
  }
};

}  // namespace blink

#endif
