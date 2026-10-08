// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/canvas_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/mailbox_ref.h"
#include "third_party/blink/renderer/platform/graphics/mailbox_texture_backing.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
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
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::PlatformThreadRef context_thread_ref,
    scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
    viz::ReleaseCallback release_callback) {
  return base::AdoptRef(new AcceleratedStaticBitmapImage(
      std::move(shared_image), sync_token, alpha_type, hdr_metadata,
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
    const gfx::HDRMetadata& hdr_metadata,
    base::OnceCallback<void(gpu::SharedImageExportResult)> external_callback) {
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
  if (!shared_image) {
    return nullptr;
  }
  auto release_token = sii->GenVerifiedSyncToken();
  // No need to keep the original image after the new reference has been added.
  // Need to update the sync token, however.
  std::move(external_callback).Run(shared_image->EndImport(release_token));

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
      std::move(shared_image), sync_token, alpha_type, hdr_metadata,
      ImageOrientationEnum::kDefault, shared_gpu_context,
      base::PlatformThreadRef(),
      ThreadScheduler::Current()->CleanupTaskRunner(),
      std::move(release_callback)));
}

// static
scoped_refptr<StaticBitmapImage> AcceleratedStaticBitmapImage::CreateFromRaster(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    gpu::SharedImageUsageSet shared_image_usage_flags,
    base::FunctionRef<void(cc::PaintCanvas&)> draw_callback,
    scoped_refptr<const cc::AnimatedImageFrameIndexMap>
        animated_image_frame_index_map) {
  // IsGpuCompositingEnabled can re-create the context if it has been lost, do
  // this up front so that we can fail early and not expose ourselves to
  // use after free bugs (crbug.com/1126424)
  const bool is_gpu_compositing_enabled =
      SharedGpuContext::IsGpuCompositingEnabled();

  // If the context is lost we don't want to re-create it here, the resulting
  // resource provider would be invalid anyway
  if (!context_provider_wrapper ||
      !context_provider_wrapper->ContextProvider().RasterInterface() ||
      context_provider_wrapper->ContextProvider().IsContextLost()) {
    return nullptr;
  }

  const auto& capabilities =
      context_provider_wrapper->ContextProvider().GetCapabilities();
  if ((size.width() < 1 || size.height() < 1 ||
       size.width() > capabilities.max_texture_size ||
       size.height() > capabilities.max_texture_size)) {
    return nullptr;
  }

  // TODO(crbug.com/40767377): Pass in info as is for all cases.
  // Overriding the info to use RGBA instead of N32 is needed because code
  // elsewhere assumes RGBA. OTOH the software path seems to be assuming N32
  // somewhere in the later pipeline but for offscreen canvas only.
  bool should_force_bgra8_to_rgba =
      !shared_image_usage_flags.Has(gpu::SHARED_IMAGE_USAGE_WEBGPU_READ);
#if BUILDFLAG(IS_WIN)
  // Concurrent read/write on Windows results in a swapchain backing, which
  // supports BGRA; hence there is no need to force to RGBA in this case.
  should_force_bgra8_to_rgba =
      should_force_bgra8_to_rgba &&
      !shared_image_usage_flags.Has(
          gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE);
#endif

#if BUILDFLAG(IS_LINUX)
  // WebGpu preferred canvas on linux is RGBA and interop (vk on gl) is
  // dependent on canvas copies being RGBA (not BGRA).
  should_force_bgra8_to_rgba = true;
#endif

  if (format != viz::SinglePlaneFormat::kRGBA_F16 &&
      should_force_bgra8_to_rgba) {
    format = viz::SinglePlaneFormat::kRGBA_8888;
  }

  const bool is_mappable_shared_image_allowed =
      is_gpu_compositing_enabled &&
      IsScanoutSupportedForCanvasWithFormat(format, capabilities);

  // If we cannot use overlay, we have to remove the scanout flag and the
  // concurrent read write flag.
  const auto& shared_image_caps = context_provider_wrapper->ContextProvider()
                                      .SharedImageInterface()
                                      ->GetCapabilities();
  bool is_overlay_supported = is_mappable_shared_image_allowed &&
                              shared_image_caps.supports_scanout_shared_images;

#if BUILDFLAG(IS_WIN)
  // On Windows, SCANOUT usage is additionally supported in the special case
  // of the swapchain being used on the service side to implement concurrent
  // read/write.
  is_overlay_supported = is_overlay_supported ||
                         (shared_image_usage_flags.Has(
                              gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE) &&
                          shared_image_caps.shared_image_swap_chain);
#endif

  if (!is_overlay_supported) {
    shared_image_usage_flags.RemoveAll(
        gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE |
        gpu::SHARED_IMAGE_USAGE_SCANOUT);
  }

#if BUILDFLAG(IS_MAC)
  if (shared_image_usage_flags.Has(gpu::SHARED_IMAGE_USAGE_SCANOUT) &&
      format == viz::SinglePlaneFormat::kRGBA_8888) {
    // GPU-accelerated scannout usage on Mac uses IOSurface.  Must switch from
    // RGBA_8888 to BGRA_8888 in that case.
    format = viz::SinglePlaneFormat::kBGRA_8888;
  }
#endif

  // These SharedImages are both read and written by the raster interface
  // (both occur, for example, when copying canvas resources between
  // canvases).
  shared_image_usage_flags = shared_image_usage_flags |
                             gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                             gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
  // Add WEBGPU_READ usage to allow importing into WebGPU without a copy.
  if (base::FeatureList::IsEnabled(kCanvasResourceIsWebGPUCompatible)) {
    shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;
  }

  auto shared_image =
      context_provider_wrapper->ContextProvider()
          .SharedImageInterface()
          ->CreateSharedImage(
              {format, size, color_space, kTopLeft_GrSurfaceOrigin, alpha_type,
               shared_image_usage_flags, "CanvasResourceRaster"},
              gpu::kNullSurfaceHandle);

  MemoryManagedPaintRecorder recorder(size, /*client=*/nullptr);
  draw_callback(recorder.getRecordingCanvas());

  cc::ImageDecodeCache* cache_f16 = nullptr;
  if (shared_image->format() == viz::SinglePlaneFormat::kRGBA_F16) {
    cache_f16 = context_provider_wrapper->ContextProvider().ImageDecodeCache(
        kRGBA_F16_SkColorType);
  }
  cc::ImageDecodeCache* cache_rgba8 =
      context_provider_wrapper->ContextProvider().ImageDecodeCache(
          kN32_SkColorType);
  CanvasImageProvider image_provider(
      cache_rgba8, cache_f16, color_space, shared_image->format(),
      cc::PlaybackImageProvider::RasterMode::kGpu, context_provider_wrapper);
  if (animated_image_frame_index_map) {
    image_provider.SetAnimatedImageFrameIndexes(
        std::move(animated_image_frame_index_map));
  }

  gpu::SyncToken sync_token =
      context_provider_wrapper->ContextProvider()
          .RasterInterface()
          ->RasterSharedImage(
              shared_image, shared_image->creation_sync_token(),
              recorder.ReleaseMainRecording(), &image_provider,
              /*needs_clear=*/true);
  image_provider.ReleaseLockedImages();
  image_provider.UnbindTextureBackedImages();

  if (context_provider_wrapper->ContextProvider().IsContextLost()) {
    return nullptr;
  }

  auto release_callback = blink::BindOnce(
      [](scoped_refptr<gpu::ClientSharedImage> shared_image,
         const gpu::SyncToken& sync_token, bool is_lost) {
        if (sync_token.HasData()) {
          shared_image->UpdateDestructionSyncToken(sync_token);
        }
      },
      shared_image);

  return CreateFromCanvasSharedImage(
      std::move(shared_image), sync_token, alpha_type, hdr_metadata,
      std::move(context_provider_wrapper), base::PlatformThread::CurrentRef(),
      ThreadScheduler::Current()->CleanupTaskRunner(),
      std::move(release_callback));
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& sync_token,
    SkAlphaType alpha_type,
    const gfx::HDRMetadata& hdr_metadata,
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
      paint_image_content_id_(cc::PaintImage::GetNextContentId()),
      hdr_metadata_(hdr_metadata) {
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
      .set_hdr_metadata(hdr_metadata_)
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
  if (paint_image.decoding_mode() != paint_image_decoding_mode) {
    paint_image =
        PaintImageBuilder::WithCopy(std::move(paint_image))
            .set_decoding_mode(paint_image_decoding_mode)
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
      shared_image_, mailbox_ref_, GetAlphaType(),
      base::WrapRefCounted<viz::RasterContextProvider>(
          context_provider_wrapper->ContextProvider().RasterContextProvider()));
}

void AcceleratedStaticBitmapImage::EnsureSyncTokenVerified() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (mailbox_ref_->verified_flush())
    return;

  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper)
    return;

  auto sync_token = mailbox_ref_->sync_token();
  int8_t* token_data = sync_token.GetData();
  context_provider_wrapper->ContextProvider()
      .InterfaceBase()
      ->VerifySyncTokensCHROMIUM(&token_data, 1);
  sync_token.SetVerifyFlush();
  mailbox_ref_->set_sync_token(sync_token);
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

void AcceleratedStaticBitmapImage::UpdateSyncTokenFromExportResult(
    gpu::SharedImageExportResult export_result) {
  if (shared_image_) {
    UpdateSyncToken(shared_image_->EndExport(std::move(export_result)));
  }
}

}  // namespace blink
