// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_STATIC_BITMAP_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_STATIC_BITMAP_IMAGE_H_

#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace blink {

class WebGraphicsContext3DProviderWrapper;

class PLATFORM_EXPORT StaticBitmapImage : public Image {
 public:
  // WebGraphicsContext3DProviderWrapper argument only needs to be provided if
  // The SkImage is texture backed, in which case it must be a reference to the
  // context provider that owns the GrContext with which the SkImage is
  // associated.
  static scoped_refptr<StaticBitmapImage> Create(
      sk_sp<SkImage>,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper> = nullptr);
  static scoped_refptr<StaticBitmapImage> Create(PaintImage);
  static scoped_refptr<StaticBitmapImage> Create(sk_sp<SkData> data,
                                                 const SkImageInfo&);

  bool IsStaticBitmapImage() const override { return true; }

  // Methods overridden by all sub-classes
  ~StaticBitmapImage() override = default;
  // Creates a gpu copy of the image using the given ContextProvider. Should
  // not be called if IsTextureBacked() is already true. May return null if the
  // conversion failed (for instance if the context had an error).
  virtual scoped_refptr<StaticBitmapImage> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_wrapper) = 0;

  // Methods have common implementation for all sub-classes
  bool CurrentFrameIsComplete() override { return true; }
  void DestroyDecodedData() override {}

  // Methods that have a default implementation, and overridden by only one
  // sub-class
  virtual bool HasMailbox() const { return false; }
  virtual bool IsValid() const { return true; }
  virtual void Transfer() {}

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

  // EnsureMailbox modifies the internal state of an accelerated static bitmap
  // image to make sure that it is represented by a Mailbox.  This must be
  // called whenever the image is intende to be used in a differen GPU context.
  // It is important to use the correct MailboxSyncMode in order to achieve
  // optimal performance without compromising security or causeing graphics
  // glitches.  Here is how to select the aprropriate mode:
  //
  // Case 1: Passing to a gpu context that is on a separate channel.
  //   Note: a context in a separate process is necessarily on another channel.
  //   Use kVerifiedSyncToken.  Or use kUnverifiedSyncToken with a later call
  //   to VerifySyncTokensCHROMIUM()
  // Case 2: Passing to a gpu context that is on the same channel but not the
  //     same stream.
  //   Use kUnverifiedSyncToken
  // Case 3: Passing to a gpu context on the same stream.
  //   Use kOrderingBarrier
  virtual void EnsureMailbox(MailboxSyncMode, GLenum filter) { NOTREACHED(); }
  virtual const gpu::Mailbox& GetMailbox() const {
    NOTREACHED();
    static const gpu::Mailbox mailbox;
    return mailbox;
  }
  virtual const gpu::SyncToken& GetSyncToken() const;
  virtual bool IsPremultiplied() const { return true; }

  // Methods have exactly the same implementation for all sub-classes
  bool OriginClean() const { return is_origin_clean_; }
  void SetOriginClean(bool flag) { is_origin_clean_ = flag; }
  scoped_refptr<StaticBitmapImage> ConvertToColorSpace(
      sk_sp<SkColorSpace>,
      SkColorType = kN32_SkColorType);

  static size_t GetSizeInBytes(const IntRect& rect,
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
                  const PaintImage&);

  // The following property is here because the SkImage API doesn't expose the
  // info. It is applied to both UnacceleratedStaticBitmapImage and
  // AcceleratedStaticBitmapImage. To change this property, the call site would
  // have to call SetOriginClean().
  bool is_origin_clean_ = true;
};

DEFINE_IMAGE_TYPE_CASTS(StaticBitmapImage);

}  // namespace blink

#endif
