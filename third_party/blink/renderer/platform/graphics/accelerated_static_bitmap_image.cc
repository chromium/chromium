// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/mailbox_ref.h"
#include "third_party/blink/renderer/platform/graphics/mailbox_texture_backing.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"

namespace blink {

// static
scoped_refptr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& sync_token,
    SkAlphaType alpha_type,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::PlatformThreadRef context_thread_ref,
    scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
    viz::ReleaseCallback release_callback) {
  return base::AdoptRef(new AcceleratedStaticBitmapImage(
      std::move(shared_image), sync_token, alpha_type,
      ImageOrientationEnum::kDefault, std::move(context_provider_wrapper),
      context_thread_ref, std::move(context_task_runner),
      std::move(release_callback)));
}

// static
scoped_refptr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::CreateFromExternalSharedImage(
    gpu::ExportedSharedImage exported_shared_image,
    const gpu::SyncToken& sync_token,
    SkAlphaType alpha_type,
    base::OnceCallback<void(const gpu::SyncToken&)> external_callback) {
  auto shared_gpu_context = blink::SharedGpuContext::ContextProviderWrapper();
  if (!shared_gpu_context) {
    return nullptr;
  }
  auto* sii = shared_gpu_context->ContextProvider().SharedImageInterface();
  if (!sii) {
    return nullptr;
  }

  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->ImportSharedImage(std::move(exported_shared_image));
  auto release_token = sii->GenVerifiedSyncToken();
  // No need to keep the original image after the new reference has been added.
  // Need to update the sync token, however.
  std::move(external_callback).Run(release_token);

  auto release_callback = blink::BindOnce(
      [](base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
         scoped_refptr<gpu::ClientSharedImage> shared_image,
         const gpu::SyncToken& sync_token, bool is_lost) {
        if (is_lost || !context_provider) {
          return;
        }
        shared_image->UpdateDestructionSyncToken(sync_token);
      },
      shared_gpu_context, shared_image);

  return base::AdoptRef(new AcceleratedStaticBitmapImage(
      std::move(shared_image), sync_token, alpha_type,
      ImageOrientationEnum::kDefault, shared_gpu_context,
      base::PlatformThreadRef(),
      ThreadScheduler::Current()->CleanupTaskRunner(),
      std::move(release_callback)));
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& sync_token,
    SkAlphaType alpha_type,
    const ImageOrientation& orientation,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::PlatformThreadRef context_thread_ref,
    scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
    viz::ReleaseCallback release_callback)
    : StaticBitmapImage(orientation),
      shared_image_(std::move(shared_image)),
      alpha_type_(alpha_type),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      mailbox_ref_(
          base::MakeRefCounted<MailboxRef>(sync_token,
                                           context_thread_ref,
                                           std::move(context_task_runner),
                                           std::move(release_callback))),
      paint_image_content_id_(cc::PaintImage::GetNextContentId()) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

AcceleratedStaticBitmapImage::~AcceleratedStaticBitmapImage() {
  // It's ok for the image to be destroyed on another thread. Unfortunately,
  // this is unavoidable for images that are snapshotted from OffscreenCanvas on
  // a worker thread and bound for destruction in a callback posted back to the
  // thread since the worker thread can be destroyed before the callback is
  // posted. In that case, the image bound to the callback is destroyed when the
  // callback's bind state is destroyed immediately after the PostTask fails.
  // This is safe because we perform no thread/sequence-affine operations here:
  // 1) we don't dereference any weak ptrs, 2) the only way the above scenario
  // can occur is if this image was transferred to another thread, in which case
  // `texture_backing_` should be null and 3) the DestroySharedImage() call in
  // the `shared_image_` destructor is thread-safe.
  DETACH_FROM_THREAD(thread_checker_);
}

scoped_refptr<StaticBitmapImage>
AcceleratedStaticBitmapImage::MakeUnaccelerated() {
  CreateImageFromMailboxIfNeeded();
  return UnacceleratedStaticBitmapImage::Create(
      PaintImageForCurrentFrame().GetSwSkImage(), orientation_);
}

bool AcceleratedStaticBitmapImage::CopyToTexture(
    gpu::gles2::GLES2Interface* dest_gl,
    GLenum dest_target,
    GLuint dest_texture_id,
    GLint dest_level,
    SkAlphaType dest_alpha_type,
    GrSurfaceOrigin destination_origin,
    const gfx::Point& dest_point,
    const gfx::Rect& src_rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsValid())
    return false;

  // This method should only be used for cross-context copying, otherwise it's
  // wasting overhead.
  DCHECK(mailbox_ref_->is_cross_thread() ||
         dest_gl != ContextProvider()->ContextGL());

  // Create a texture that |destProvider| knows about and copy from it.
  auto source_si_texture = shared_image_->CreateGLTexture(dest_gl);
  auto source_scoped_si_access = source_si_texture->BeginAccess(
      mailbox_ref_->sync_token(), /*readonly=*/true);
  const bool do_alpha_multiply = GetAlphaType() == kUnpremul_SkAlphaType &&
                                 dest_alpha_type == kPremul_SkAlphaType;
  const bool do_alpha_unmultiply = GetAlphaType() == kPremul_SkAlphaType &&
                                   dest_alpha_type == kUnpremul_SkAlphaType;

  // `src_rect` here is always in top-left coordinate space, but
  // CopySubTextureCHROMIUM source rect is in texture coordinate space, so we
  // need to adjust.
  auto source_sub_rectangle = src_rect;
  if (shared_image_->surface_origin() == kBottomLeft_GrSurfaceOrigin) {
    source_sub_rectangle.set_y(Size().height() - source_sub_rectangle.bottom());
  }

  // If source origin doesn't match destination, we need to flip.
  bool unpack_flip_y = shared_image_->surface_origin() != destination_origin;

  dest_gl->CopySubTextureCHROMIUM(
      source_scoped_si_access->texture_id(), 0, dest_target, dest_texture_id,
      dest_level, dest_point.x(), dest_point.y(), source_sub_rectangle.x(),
      source_sub_rectangle.y(), source_sub_rectangle.width(),
      source_sub_rectangle.height(), unpack_flip_y,
      do_alpha_multiply ? GL_TRUE : GL_FALSE,
      do_alpha_unmultiply ? GL_TRUE : GL_FALSE);
  auto sync_token = gpu::SharedImageTexture::ScopedAccess::EndAccess(
      std::move(source_scoped_si_access));

  // We need to update the texture holder's sync token to ensure that when this
  // mailbox is recycled or deleted, it is done after the copy operation above.
  mailbox_ref_->set_sync_token(sync_token);

  return true;
}

bool AcceleratedStaticBitmapImage::CopyToResourceProvider(
    CanvasResourceProviderSharedImage* resource_provider,
    const gfx::Rect& copy_rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(resource_provider);

  if (!IsValid())
    return false;

  const gpu::SyncToken& ready_sync_token = mailbox_ref_->sync_token();
  gpu::SyncToken completion_sync_token;
  if (!resource_provider->OverwriteImage(
          shared_image_, copy_rect, ready_sync_token, completion_sync_token)) {
    return false;
  }

  // We need to update the texture holder's sync token to ensure that when this
  // mailbox is recycled or deleted, it is done after the copy operation above.
  mailbox_ref_->set_sync_token(completion_sync_token);
  return true;
}

PaintImage AcceleratedStaticBitmapImage::PaintImageForCurrentFrame() {
  // TODO(ccameron): This function should not ignore |colorBehavior|.
  // https://crbug.com/672306
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsValid())
    return PaintImage();

  CreateImageFromMailboxIfNeeded();

  return CreatePaintImageBuilder()
      .set_texture_backing(texture_backing_, paint_image_content_id_)
      .set_completion_state(PaintImage::CompletionState::kDone)
      .TakePaintImage();
}

void AcceleratedStaticBitmapImage::Draw(cc::PaintCanvas* canvas,
                                        const cc::PaintFlags& flags,
                                        const gfx::RectF& dst_rect,
                                        const gfx::RectF& src_rect,
                                        const ImageDrawOptions& draw_options) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto paint_image = PaintImageForCurrentFrame();
  if (!paint_image)
    return;
  auto paint_image_decoding_mode =
      ToPaintImageDecodingMode(draw_options.decode_mode);
  if (paint_image.decoding_mode() != paint_image_decoding_mode ||
      paint_image.may_be_lcp_candidate() != draw_options.may_be_lcp_candidate) {
    paint_image =
        PaintImageBuilder::WithCopy(std::move(paint_image))
            .set_decoding_mode(paint_image_decoding_mode)
            .set_may_be_lcp_candidate(draw_options.may_be_lcp_candidate)
            .TakePaintImage();
  }
  StaticBitmapImage::DrawHelper(canvas, flags, dst_rect, src_rect, draw_options,
                                paint_image);
}

bool AcceleratedStaticBitmapImage::IsValid() const {
  if (mailbox_ref_->is_cross_thread()) {
    // If context is is from another thread, validity cannot be verified. Just
    // assume valid. Potential problem will be detected later.
    return true;
  }

  // Check the weak pointers after checking that the image is not cross thread
  // as weak pointers validity cannot be checked on multiple threads.
  if (texture_backing_ && !skia_context_provider_wrapper_) {
    return false;
  }

  return !!context_provider_wrapper_;
}

WebGraphicsContext3DProvider* AcceleratedStaticBitmapImage::ContextProvider()
    const {
  auto context = ContextProviderWrapper();
  return context ? &(context->ContextProvider()) : nullptr;
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
AcceleratedStaticBitmapImage::ContextProviderWrapper() const {
  return texture_backing_ ? skia_context_provider_wrapper_
                          : context_provider_wrapper_;
}

void AcceleratedStaticBitmapImage::CreateImageFromMailboxIfNeeded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (texture_backing_)
    return;

  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper)
    return;

  skia_context_provider_wrapper_ = context_provider_wrapper;
  texture_backing_ = sk_make_sp<MailboxTextureBacking>(
      shared_image_, mailbox_ref_, GetSize(), GetSharedImageFormat(),
      GetAlphaType(), GetColorSpace(),
      base::WrapRefCounted<viz::RasterContextProvider>(
          context_provider_wrapper->ContextProvider().RasterContextProvider()));
}

void AcceleratedStaticBitmapImage::EnsureSyncTokenVerified() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (mailbox_ref_->verified_flush())
    return;

  // If the original context was created on a different thread, we need to
  // fallback to using the shared GPU context.
  auto context_provider_wrapper =
      mailbox_ref_->is_cross_thread()
          ? SharedGpuContext::ContextProviderWrapper()
          : ContextProviderWrapper();
  if (!context_provider_wrapper)
    return;

  auto sync_token = mailbox_ref_->sync_token();
  int8_t* token_data = sync_token.GetData();
  ContextProvider()->InterfaceBase()->VerifySyncTokensCHROMIUM(&token_data, 1);
  sync_token.SetVerifyFlush();
  mailbox_ref_->set_sync_token(sync_token);
}

gpu::MailboxHolder AcceleratedStaticBitmapImage::GetMailboxHolder() const {
  if (!IsValid()) {
    return gpu::MailboxHolder();
  }
  return gpu::MailboxHolder(shared_image_->mailbox(),
                            mailbox_ref_->sync_token(),
                            shared_image_->GetTextureTarget());
}

scoped_refptr<gpu::ClientSharedImage>
AcceleratedStaticBitmapImage::GetSharedImage() const {
  if (!IsValid()) {
    return nullptr;
  }
  return shared_image_;
}

gpu::SyncToken AcceleratedStaticBitmapImage::GetSyncToken() const {
  if (!IsValid()) {
    return gpu::SyncToken();
  }
  return mailbox_ref_->sync_token();
}

void AcceleratedStaticBitmapImage::Transfer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // SkImage is bound to the current thread so is no longer valid to use
  // cross-thread.
  texture_backing_.reset();
  skia_context_provider_wrapper_.reset();

  DETACH_FROM_THREAD(thread_checker_);
}

bool AcceleratedStaticBitmapImage::IsOpaque() {
  return SkAlphaTypeIsOpaque(GetAlphaType()) ||
         !GetSharedImageFormat().HasAlpha();
}

}  // namespace blink
