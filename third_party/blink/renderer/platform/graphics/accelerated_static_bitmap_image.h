// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_ACCELERATED_STATIC_BITMAP_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_ACCELERATED_STATIC_BITMAP_IMAGE_H_

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/resources/release_callback.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/renderer/platform/graphics/mailbox_ref.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace gpu {
class ClientSharedImage;
struct ExportedSharedImage;
}  // namespace gpu

namespace blink {
class MailboxTextureBacking;
class WebGraphicsContext3DProviderWrapper;

class PLATFORM_EXPORT AcceleratedStaticBitmapImage final
    : public StaticBitmapImage {
 public:
  ~AcceleratedStaticBitmapImage() override;

  // Creates an image wrapping a shared image.
  //
  // |sync_token| is the token that must be waited on before reading the
  // contents of this shared image.
  //
  // |context_provider| is the context that the shared image was created with.
  // |context_thread_ref| and |context_task_runner| refer to the thread the
  // context is bound to. If the image is created on a different thread than
  // |context_thread_ref| then the provided sync_token must be verified and no
  // |shared_image_texture_id| should be provided.
  //
  // |release_callback| is a callback to be invoked when this shared image can
  // be safely destroyed. It is guaranteed to be invoked on the context thread.
  //
  // Note that it is assumed that the mailbox can only be used for read
  // operations, no writes are allowed.
  static scoped_refptr<AcceleratedStaticBitmapImage>
  CreateFromCanvasSharedImage(
      scoped_refptr<gpu::ClientSharedImage>,
      const gpu::SyncToken&,
      SkAlphaType alpha_type,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::PlatformThreadRef context_thread_ref,
      scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
      viz::ReleaseCallback release_callback);

  // Creates an image wrapping an external shared image.
  // The shared image may come from a different context,
  // potentially from a different process.
  // This takes ownership of the shared image.
  static scoped_refptr<AcceleratedStaticBitmapImage>
  CreateFromExternalSharedImage(
      gpu::ExportedSharedImage exported_shared_image,
      const gpu::SyncToken& sync_token,
      SkAlphaType alpha_type,
      base::OnceCallback<void(const gpu::SyncToken&)> release_callback);

  bool IsOpaque() override;
  bool IsTextureBacked() const override { return true; }

  void Draw(cc::PaintCanvas*,
            const cc::PaintFlags&,
            const gfx::RectF& dst_rect,
            const gfx::RectF& src_rect,
            const ImageDrawOptions&) override;

  bool IsValid() const final;
  WebGraphicsContext3DProvider* ContextProvider() const final;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const final;
  scoped_refptr<StaticBitmapImage> MakeUnaccelerated() final;

  bool CopyToTexture(gpu::gles2::GLES2Interface* dest_gl,
                     GLenum dest_target,
                     GLuint dest_texture_id,
                     GLint dest_level,
                     SkAlphaType dest_alpha_type,
                     GrSurfaceOrigin destination_origin,
                     const gfx::Point& dest_point,
                     const gfx::Rect& src_rect) override;

  bool CopyToResourceProvider(
      CanvasResourceProviderSharedImage* resource_provider,
      const gfx::Rect& copy_rect) override;

  // To be called on sender thread before performing a transfer to a different
  // thread.
  void Transfer() final;

  // Makes sure that the sync token associated with this mailbox is verified.
  void EnsureSyncTokenVerified() final;

  // Updates the sync token that must be waited on before recycling or deleting
  // the mailbox for this image. This must be set by callers using the mailbox
  // externally to this class.
  void UpdateSyncToken(const gpu::SyncToken& sync_token) final {
    mailbox_ref_->set_sync_token(sync_token);
  }

  // Provides the mailbox backing for this image. The caller must wait on the
  // sync token before accessing this mailbox.
  gpu::MailboxHolder GetMailboxHolder() const final;
  scoped_refptr<gpu::ClientSharedImage> GetSharedImage() const final;
  gpu::SyncToken GetSyncToken() const final;

  PaintImage PaintImageForCurrentFrame() override;

  gfx::Size GetSize() const override { return shared_image_->size(); }
  SkAlphaType GetAlphaType() const override { return alpha_type_; }
  gfx::ColorSpace GetColorSpace() const override {
    return shared_image_->color_space();
  }
  viz::SharedImageFormat GetSharedImageFormat() const override {
    return shared_image_->format();
  }

 private:
  AcceleratedStaticBitmapImage(
      scoped_refptr<gpu::ClientSharedImage>,
      const gpu::SyncToken&,
      SkAlphaType alpha_type,
      const ImageOrientation& orientation,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::PlatformThreadRef context_thread_ref,
      scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
      viz::ReleaseCallback release_callback);

  void CreateImageFromMailboxIfNeeded();

  // Fields must be safe to destroy on any thread or invalidtaed in Transfer().
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  SkAlphaType alpha_type_;

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  scoped_refptr<MailboxRef> mailbox_ref_;

  // The context `texture_backing_` is bound to. This and `texture_backing_` are
  // thread-affine state that must be invalidated in Transfer().
  base::WeakPtr<WebGraphicsContext3DProviderWrapper>
      skia_context_provider_wrapper_;
  sk_sp<MailboxTextureBacking> texture_backing_;

  PaintImage::ContentId paint_image_content_id_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_ACCELERATED_STATIC_BITMAP_IMAGE_H_
