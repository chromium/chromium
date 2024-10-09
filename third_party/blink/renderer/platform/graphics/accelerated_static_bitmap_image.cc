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
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"

namespace blink {

// static
void AcceleratedStaticBitmapImage::ReleaseTexture(void* ctx) {
  auto* release_ctx = static_cast<ReleaseContext*>(ctx);
  if (release_ctx->context_provider_wrapper) {
    if (release_ctx->texture_id) {
      auto* ri = release_ctx->context_provider_wrapper->ContextProvider()
                     ->RasterInterface();
      ri->EndSharedImageAccessDirectCHROMIUM(release_ctx->texture_id);
      ri->DeleteGpuRasterTexture(release_ctx->texture_id);
    }
  }

  delete release_ctx;
}

// static
scoped_refptr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& sync_token,
    GLuint shared_image_texture_id,
    const SkImageInfo& sk_image_info,
    GLenum texture_target,
    bool is_origin_top_left,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::PlatformThreadRef context_thread_ref,
    scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
    viz::ReleaseCallback release_callback,
    bool supports_display_compositing,
    bool is_overlay_candidate) {
  return base::AdoptRef(new AcceleratedStaticBitmapImage(
      std::move(shared_image), sync_token, shared_image_texture_id,
      sk_image_info, texture_target, is_origin_top_left,
      supports_display_compositing, is_overlay_candidate,
      ImageOrientationEnum::kDefault, std::move(context_provider_wrapper),
      context_thread_ref, std::move(context_task_runner),
      std::move(release_callback)));
}

// static
scoped_refptr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::CreateFromExternalSharedImage(
    const gpu::ExportedSharedImage& exported_shared_image,
    const SkImageInfo& sk_image_info,
    bool is_origin_top_left,
    bool supports_display_compositing,
    bool is_overlay_candidate,
    base::OnceCallback<void(const gpu::SyncToken&)> external_callback) {
  auto shared_gpu_context = blink::SharedGpuContext::ContextProviderWrapper();
  if (!shared_gpu_context) {
    return nullptr;
  }
  auto* sii = shared_gpu_context->ContextProvider()->SharedImageInterface();
  if (!sii) {
    return nullptr;
  }

  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->ImportSharedImage(exported_shared_image);
  auto release_token = sii->GenVerifiedSyncToken();
  // No need to keep the original image after the new reference has been added.
  // Need to update the sync token, however.
  std::move(external_callback).Run(release_token);

  auto release_callback = WTF::BindOnce(
      [](base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
         scoped_refptr<gpu::ClientSharedImage> shared_image,
         const gpu::SyncToken& sync_token, bool is_lost) {
        if (is_lost || !context_provider) {
          return;
        }
        shared_image->UpdateDestructionSyncToken(sync_token);
      },
      shared_gpu_context, shared_image);

  auto texture_target = shared_image->GetTextureTarget();

  return base::AdoptRef(new AcceleratedStaticBitmapImage(
      std::move(shared_image), release_token, 0u, sk_image_info, texture_target,
      is_origin_top_left, supports_display_compositing, is_overlay_candidate,
      ImageOrientationEnum::kDefault, shared_gpu_context,
      base::PlatformThreadRef(),
      ThreadScheduler::Current()->CleanupTaskRunner(),
      std::move(release_callback)));
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& sync_token,
    GLuint shared_image_texture_id,
    const SkImageInfo& sk_image_info,
    GLenum texture_target,
    bool is_origin_top_left,
    bool supports_display_compositing,
    bool is_overlay_candidate,
    const ImageOrientation& orientation,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::PlatformThreadRef context_thread_ref,
    scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
    viz::ReleaseCallback release_callback)
    : StaticBitmapImage(orientation),
      shared_image_(std::move(shared_image)),
      sk_image_info_(sk_image_info),
      texture_target_(texture_target),
      is_origin_top_left_(is_origin_top_left),
      supports_display_compositing_(supports_display_compositing),
      is_overlay_candidate_(is_overlay_candidate),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      mailbox_ref_(
          base::MakeRefCounted<MailboxRef>(sync_token,
                                           context_thread_ref,
                                           std::move(context_task_runner),
                                           std::move(release_callback))),
      paint_image_content_id_(cc::PaintImage::GetNextContentId()) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (shared_image_texture_id)
    InitializeTextureBacking(shared_image_texture_id);
}

AcceleratedStaticBitmapImage::~AcceleratedStaticBitmapImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

SkImageInfo AcceleratedStaticBitmapImage::GetSkImageInfo() const {
  return sk_image_info_;
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
    bool unpack_premultiply_alpha,
    bool unpack_flip_y,
    const gfx::Point& dest_point,
    const gfx::Rect& source_sub_rectangle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsValid())
    return false;

  // This method should only be used for cross-context copying, otherwise it's
  // wasting overhead.
  DCHECK(mailbox_ref_->is_cross_thread() ||
         dest_gl != ContextProvider()->ContextGL());

  // Get a texture id that |destProvider| knows about and copy from it.
  dest_gl->WaitSyncTokenCHROMIUM(mailbox_ref_->sync_token().GetConstData());
  GLuint source_texture_id = dest_gl->CreateAndTexStorage2DSharedImageCHROMIUM(
      shared_image_->mailbox().name);
  dest_gl->BeginSharedImageAccessDirectCHROMIUM(
      source_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  dest_gl->CopySubTextureCHROMIUM(
      source_texture_id, 0, dest_target, dest_texture_id, dest_level,
      dest_point.x(), dest_point.y(), source_sub_rectangle.x(),
      source_sub_rectangle.y(), source_sub_rectangle.width(),
      source_sub_rectangle.height(), unpack_flip_y,
      /*unpack_premultiply_alpha=*/GL_FALSE,
      /*unpack_unmultiply_alpha=*/
      unpack_premultiply_alpha ? GL_FALSE : GL_TRUE);
  dest_gl->EndSharedImageAccessDirectCHROMIUM(source_texture_id);
  dest_gl->DeleteTextures(1, &source_texture_id);

  // We need to update the texture holder's sync token to ensure that when this
  // mailbox is recycled or deleted, it is done after the copy operation above.
  gpu::SyncToken sync_token;
  dest_gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  mailbox_ref_->set_sync_token(sync_token);

  return true;
}

bool AcceleratedStaticBitmapImage::CopyToResourceProvider(
    CanvasResourceProvider* resource_provider,
    const gfx::Rect& copy_rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(resource_provider);

  if (!IsValid())
    return false;

  const bool unpack_flip_y =
      (IsOriginTopLeft() != resource_provider->IsOriginTopLeft());

  const gpu::SyncToken& ready_sync_token = mailbox_ref_->sync_token();
  gpu::SyncToken completion_sync_token;
  if (!resource_provider->OverwriteImage(
          shared_image_->mailbox(), copy_rect, unpack_flip_y,
          /*unpack_premultiply_alpha=*/false, ready_sync_token,
          completion_sync_token)) {
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
  if (texture_backing_ && !skia_context_provider_wrapper_)
    return false;

  if (mailbox_ref_->is_cross_thread()) {
    // If context is is from another thread, validity cannot be verified. Just
    // assume valid. Potential problem will be detected later.
    return true;
  }

  return !!context_provider_wrapper_;
}

WebGraphicsContext3DProvider* AcceleratedStaticBitmapImage::ContextProvider()
    const {
  auto context = ContextProviderWrapper();
  return context ? context->ContextProvider() : nullptr;
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
  InitializeTextureBacking(0u);
}

void AcceleratedStaticBitmapImage::InitializeTextureBacking(
    GLuint shared_image_texture_id) {
  DCHECK(!shared_image_texture_id || !mailbox_ref_->is_cross_thread());

  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper)
    return;

  gpu::raster::RasterInterface* shared_ri =
      context_provider_wrapper->ContextProvider()->RasterInterface();
  shared_ri->WaitSyncTokenCHROMIUM(mailbox_ref_->sync_token().GetConstData());

  const auto& capabilities =
      context_provider_wrapper->ContextProvider()->GetCapabilities();

  if (capabilities.gpu_rasterization) {
    DCHECK_EQ(shared_image_texture_id, 0u);
    skia_context_provider_wrapper_ = context_provider_wrapper;
    texture_backing_ = sk_make_sp<MailboxTextureBacking>(
        shared_image_->mailbox(), mailbox_ref_, sk_image_info_,
        std::move(context_provider_wrapper));
    return;
  }

  GrDirectContext* shared_gr_context =
      context_provider_wrapper->ContextProvider()->GetGrContext();
  DCHECK(shared_ri &&
         shared_gr_context);  // context isValid already checked in callers

  GLuint shared_context_texture_id = 0u;
  bool should_delete_texture_on_release = true;

  if (shared_image_texture_id) {
    shared_context_texture_id = shared_image_texture_id;
    should_delete_texture_on_release = false;
  } else {
    shared_context_texture_id =
        shared_ri->CreateAndConsumeForGpuRaster(shared_image_->mailbox());
    shared_ri->BeginSharedImageAccessDirectCHROMIUM(
        shared_context_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  }

  GrGLTextureInfo texture_info;
  texture_info.fTarget = texture_target_;
  texture_info.fID = shared_context_texture_id;
  texture_info.fFormat =
      context_provider_wrapper->ContextProvider()->GetGrGLTextureFormat(
          viz::SkColorTypeToSinglePlaneSharedImageFormat(
              sk_image_info_.colorType()));
  auto backend_texture =
      GrBackendTextures::MakeGL(sk_image_info_.width(), sk_image_info_.height(),
                                skgpu::Mipmapped::kNo, texture_info);

  GrSurfaceOrigin origin = IsOriginTopLeft() ? kTopLeft_GrSurfaceOrigin
                                             : kBottomLeft_GrSurfaceOrigin;

  auto* release_ctx = new ReleaseContext;
  release_ctx->mailbox_ref = mailbox_ref_;
  if (should_delete_texture_on_release)
    release_ctx->texture_id = shared_context_texture_id;
  release_ctx->context_provider_wrapper = context_provider_wrapper;

  sk_sp<SkImage> sk_image = SkImages::BorrowTextureFrom(
      shared_gr_context, backend_texture, origin, sk_image_info_.colorType(),
      sk_image_info_.alphaType(), sk_image_info_.refColorSpace(),
      &ReleaseTexture, release_ctx);

  if (sk_image) {
    skia_context_provider_wrapper_ = context_provider_wrapper;
    texture_backing_ = sk_make_sp<MailboxTextureBacking>(
        std::move(sk_image), mailbox_ref_, sk_image_info_,
        std::move(context_provider_wrapper));
  }
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
  if (!IsValid())
    return gpu::MailboxHolder();

  return gpu::MailboxHolder(shared_image_->mailbox(),
                            mailbox_ref_->sync_token(), texture_target_);
}

scoped_refptr<gpu::ClientSharedImage>
AcceleratedStaticBitmapImage::GetSharedImage() const {
  if (!IsValid()) {
    return nullptr;
  }

  return shared_image_;
}

void AcceleratedStaticBitmapImage::Transfer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // SkImage is bound to the current thread so is no longer valid to use
  // cross-thread.
  texture_backing_.reset();

  DETACH_FROM_THREAD(thread_checker_);
}

bool AcceleratedStaticBitmapImage::CurrentFrameKnownToBeOpaque() {
  return sk_image_info_.isOpaque();
}

scoped_refptr<StaticBitmapImage>
AcceleratedStaticBitmapImage::ConvertToColorSpace(
    sk_sp<SkColorSpace> color_space,
    SkColorType color_type) {
  SkImageInfo image_info = PaintImageForCurrentFrame().GetSkImageInfo();
  DCHECK(color_space);
  DCHECK(color_type == kRGBA_F16_SkColorType ||
         color_type == kRGBA_8888_SkColorType ||
         color_type == image_info.colorType());

  if (!ContextProviderWrapper())
    return nullptr;

  if (SkColorSpace::Equals(color_space.get(), image_info.colorSpace()) &&
      color_type == image_info.colorType()) {
    return this;
  }
  image_info = image_info.makeColorSpace(color_space)
                   .makeColorType(color_type)
                   .makeWH(Size().width(), Size().height());

  const auto usage_flags = ContextProviderWrapper()
                               ->ContextProvider()
                               ->SharedImageInterface()
                               ->UsageForMailbox(shared_image_->mailbox());
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      image_info, cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kNo, ContextProviderWrapper(),
      RasterMode::kGPU, usage_flags);
  if (!provider) {
    return nullptr;
  }

  cc::PaintFlags paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  provider->Canvas().drawImage(PaintImageForCurrentFrame(), 0, 0,
                               SkSamplingOptions(), &paint);
  return provider->Snapshot(FlushReason::kNon2DCanvas, orientation_);
}

gpu::SharedImageUsageSet AcceleratedStaticBitmapImage::GetUsage() const {
  return ContextProviderWrapper()
      ->ContextProvider()
      ->SharedImageInterface()
      ->UsageForMailbox(shared_image_->mailbox());
}

}  // namespace blink
