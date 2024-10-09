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
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

struct SkImageInfo;

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
  // |shared_image_texture_id| is an optional texture bound to the shared image
  // imported into the provided context. If provided the caller must ensure that
  // the texture is bound to the shared image, stays alive and has a read lock
  // on the shared image until the |release_callback| is invoked.
  //
  // |sk_image_info| provides the metadata associated with the backing.
  //
  // |texture_target| is the target that the texture should be bound to if the
  // backing is used with GL.
  //
  // |is_origin_top_left| indicates whether the origin in texture space
  // corresponds to the top-left content pixel.
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
      GLuint shared_image_texture_id,
      const SkImageInfo& sk_image_info,
      GLenum texture_target,
      bool is_origin_top_left,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::PlatformThreadRef context_thread_ref,
      scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
      viz::ReleaseCallback release_callback,
      bool supports_display_compositing,
      bool is_overlay_candidate);

  // Creates an image wrapping an external shared image.
  // The shared image may come from a different context,
  // potentially from a different process.
  // This takes ownership of the shared image.
  static scoped_refptr<AcceleratedStaticBitmapImage>
  CreateFromExternalSharedImage(
      const gpu::ExportedSharedImage& exported_shared_image,
      const SkImageInfo& sk_image_info,
      bool is_origin_top_left,
      bool supports_display_compositing,
      bool is_overlay_candidate,
      base::OnceCallback<void(const gpu::SyncToken&)> release_callback);

  bool CurrentFrameKnownToBeOpaque() override;
  bool IsTextureBacked() const override { return true; }
  scoped_refptr<StaticBitmapImage> ConvertToColorSpace(sk_sp<SkColorSpace>,
                                                       SkColorType) override;

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
                     bool unpack_premultiply_alpha,
                     bool unpack_flip_y,
                     const gfx::Point& dest_point,
                     const gfx::Rect& source_sub_rectangle) override;

  bool CopyToResourceProvider(CanvasResourceProvider* resource_provider,
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
  bool IsOriginTopLeft() const final { return is_origin_top_left_; }
  bool SupportsDisplayCompositing() const final {
    return supports_display_compositing_;
  }
  bool IsOverlayCandidate() const final { return is_overlay_candidate_; }

  PaintImage PaintImageForCurrentFrame() override;

  SkImageInfo GetSkImageInfo() const override;

  gpu::SharedImageUsageSet GetUsage() const override;

 private:
  struct ReleaseContext {
    scoped_refptr<MailboxRef> mailbox_ref;
    GLuint texture_id = 0u;
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper;
  };

  static void ReleaseTexture(void* ctx);

  AcceleratedStaticBitmapImage(
      scoped_refptr<gpu::ClientSharedImage>,
      const gpu::SyncToken&,
      GLuint shared_image_texture_id,
      const SkImageInfo& sk_image_info,
      GLenum texture_target,
      bool is_origin_top_left,
      bool supports_display_compositing,
      bool is_overlay_candidate,
      const ImageOrientation& orientation,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::PlatformThreadRef context_thread_ref,
      scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
      viz::ReleaseCallback release_callback);

  void CreateImageFromMailboxIfNeeded();
  void InitializeTextureBacking(GLuint shared_image_texture_id);

  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  const SkImageInfo sk_image_info_;
  const GLenum texture_target_;
  const bool is_origin_top_left_ : 1;
  const bool supports_display_compositing_ : 1;
  const bool is_overlay_candidate_ : 1;

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  scoped_refptr<MailboxRef> mailbox_ref_;

  // The context this TextureBacking is bound to.
  base::WeakPtr<WebGraphicsContext3DProviderWrapper>
      skia_context_provider_wrapper_;
  sk_sp<MailboxTextureBacking> texture_backing_;

  PaintImage::ContentId paint_image_content_id_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_ACCELERATED_STATIC_BITMAP_IMAGE_H_
