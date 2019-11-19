// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_TEXTURE_HOLDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_TEXTURE_HOLDER_H_

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/blink/renderer/platform/graphics/texture_holder.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {
class MailboxTextureHolder;
class WebGraphicsContext3DProviderWrapper;

class PLATFORM_EXPORT SkiaTextureHolder final : public TextureHolder {
 public:
  ~SkiaTextureHolder() override;

  // TextureHolder impl.
  IntSize Size() const final {
    if (image_)
      return IntSize(image_->width(), image_->height());
    return IntSize();
  }
  bool IsValid() const final;
  bool CurrentFrameKnownToBeOpaque() const final {
    return image_ && image_->isOpaque();
  }

  const sk_sp<SkImage>& GetSkImage() const { return image_; }

  // When creating a AcceleratedStaticBitmap from a texture-backed SkImage, this
  // function will be called to create a TextureHolder object.
  SkiaTextureHolder(sk_sp<SkImage>,
                    base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&);

  // This function consumes the mailbox in the input parameter and turn it into
  // a texture-backed SkImage.
  // |shared_image_texture_id| is an optional texture bound to the
  // |texture_holder|'s mailbox and imported into its context. Note that if a
  // texture is provided it must:
  // 1) Be associated with a shared image mailbox.
  // 2) Stay alive and have a read lock on the shared image until the
  //    |MailboxRef| is destroyed.
  SkiaTextureHolder(const MailboxTextureHolder* texture_holder,
                    GLuint shared_image_texture_id);

 private:
  // The image_ should always be texture-backed.
  sk_sp<SkImage> image_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_TEXTURE_HOLDER_H_
