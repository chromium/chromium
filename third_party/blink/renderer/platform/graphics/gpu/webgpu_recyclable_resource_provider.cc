// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_recyclable_resource_provider.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/display_item_list.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_feature_type.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/canvas_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/skia/include/core/SkAlphaType.h"

namespace blink {

base::WeakPtr<WebGpuRecyclableResourceProvider>
WebGpuRecyclableResourceProvider::CreateWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<WebGpuRecyclableResourceProvider>
WebGpuRecyclableResourceProvider::Create(gfx::Size size,
                                         viz::SharedImageFormat format,
                                         SkAlphaType alpha_type,
                                         const gfx::ColorSpace& color_space,
                                         const gfx::HDRMetadata& hdr_metadata) {
  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();

  // IsGpuCompositingEnabled can re-create the context if it has been lost, do
  // this up front so that we can fail early and not expose ourselves to
  // use after free bugs (crbug.com/1126424)
  std::ignore = SharedGpuContext::IsGpuCompositingEnabled();

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

#if BUILDFLAG(IS_LINUX)
  // WebGpu preferred canvas on linux is RGBA and interop (vk on gl) is
  // dependent on canvas copies being RGBA (not BGRA).
  if (format != viz::SinglePlaneFormat::kRGBA_F16) {
    format = viz::SinglePlaneFormat::kRGBA_8888;
  }
#endif

  auto provider = base::WrapUnique(new WebGpuRecyclableResourceProvider(
      size, format, alpha_type, color_space, hdr_metadata,
      context_provider_wrapper));

  return provider->IsValid() ? std::move(provider) : nullptr;
}

WebGpuRecyclableResourceProvider::WebGpuRecyclableResourceProvider(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : size_(size),
      format_(format),
      alpha_type_(alpha_type),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata),
      recorder_for_external_draws_(
          std::make_unique<MemoryManagedPaintRecorder>(Size(),
                                                       /*client=*/nullptr)),
      context_provider_wrapper_(std::move(context_provider_wrapper)) {
  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
  if (context_provider_wrapper_) {
    context_provider_wrapper_->AddObserver(this);
    raster_context_provider_ = base::WrapRefCounted(
        context_provider_wrapper_->ContextProvider().RasterContextProvider());
    // Graphite can handle a large buffer size.
    if (context_provider_wrapper_->ContextProvider()
            .GetGpuFeatureInfo()
            .status_values[gpu::GPU_FEATURE_TYPE_SKIA_GRAPHITE] ==
        gpu::kGpuFeatureStatusEnabled) {
      recorder_for_external_draws_->DisableLineDrawingAsPaths();
    }
  }

  if (raster_context_provider_) {
    raster_context_provider_->AddObserver(this);
  }

  if (context_provider_wrapper_) {
    if (auto* sii = context_provider_wrapper_->ContextProvider()
                        .SharedImageInterface()) {
      // The SharedImages created by this provider serve as a means of
      // import/export between VideoFrames/canvas and WebGPU, e.g.:
      // * Import from VideoFrames into WebGPU via CreateExternalTexture() (the
      //   WebGPU textures will then be read by clients)
      // * Export from WebGPU into a static bitmap image via
      //   GpuCanvasContext::{PaintRenderingResultsToSnapshot, GetImage}() (the
      //   export happens via the WebGPU interface)
      // Hence, both WEBGPU_READ and WEBGPU_WRITE usage are needed here.
      // Additionally, these SharedImages are both read and written by the
      // raster interface (both occur, for example, when copying canvas
      // resources between canvases) and can be put into
      // AcceleratedStaticBitmapImages (via Bitmap()) that are then copied into
      // GL textures by WebGL (via
      // AcceleratedStaticBitmapImage::CopyToTexture()).
      gpu::SharedImageUsageSet shared_image_usage_flags =
          gpu::SHARED_IMAGE_USAGE_WEBGPU_READ |
          gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE |
          gpu::SHARED_IMAGE_USAGE_RASTER_READ |
          gpu::SHARED_IMAGE_USAGE_RASTER_WRITE |
          gpu::SHARED_IMAGE_USAGE_GLES2_READ;

      auto client_shared_image = sii->CreateSharedImage(
          {format, size, color_space, kTopLeft_GrSurfaceOrigin, alpha_type,
           shared_image_usage_flags, "CanvasResourceRaster"},
          gpu::kNullSurfaceHandle);

      if (client_shared_image) {
        resource_ = base::MakeRefCounted<CanvasResourceSharedImage>(
            std::move(client_shared_image));
        resource_->Initialize(CreateWeakPtr(), context_provider_wrapper_,
                              hdr_metadata_, /*is_accelerated=*/true);
        ++num_inflight_resources_;
        if (num_inflight_resources_ > max_inflight_resources_) {
          max_inflight_resources_ = num_inflight_resources_;
        }
      }
    }
  }

  if (resource_) {
    EnsureWriteAccess();
  }
}

WebGpuRecyclableResourceProvider::~WebGpuRecyclableResourceProvider() {
  CanvasMemoryDumpProvider::Instance()->UnregisterClient(this);
  if (context_provider_wrapper_) {
    context_provider_wrapper_->RemoveObserver(this);
  }
  if (raster_context_provider_) {
    raster_context_provider_->RemoveObserver(this);
  }

  UMA_HISTOGRAM_EXACT_LINEAR("Blink.Canvas.MaximumInflightResources",
                             max_inflight_resources_, 20);
}

bool WebGpuRecyclableResourceProvider::IsValid() const {
  return !IsGpuContextLost();
}

void WebGpuRecyclableResourceProvider::EnsureWriteAccess() {
  DCHECK(resource_);
  DCHECK(!resource()->is_cross_thread())
      << "Write access is only allowed on the owning thread";

  if (current_resource_has_write_access_ || IsGpuContextLost()) {
    return;
  }
  current_resource_has_write_access_ = true;
}

void WebGpuRecyclableResourceProvider::EndWriteAccess() {
  DCHECK(!resource()->is_cross_thread());

  if (!current_resource_has_write_access_ || IsGpuContextLost()) {
    return;
  }

  current_resource_has_write_access_ = false;
}

void WebGpuRecyclableResourceProvider::OnContextLost() {
  if (notified_context_lost_) {
    return;
  }
  notified_context_lost_ = true;
}

void WebGpuRecyclableResourceProvider::OnContextDestroyed() {
  canvas_image_provider_.reset();
}

// For WebGpu RecyclableCanvasResource.
void WebGpuRecyclableResourceProvider::OnAcquireRecyclableCanvasResource() {
  EnsureWriteAccess();
}

void WebGpuRecyclableResourceProvider::OnDestroyRecyclableCanvasResource(
    const gpu::SyncToken& sync_token) {
  resource()->WaitSyncToken(sync_token);
}

void WebGpuRecyclableResourceProvider::OnResourceRefReturned(
    scoped_refptr<CanvasResourceSharedImage>&& resource) {}

gpu::raster::RasterInterface*
WebGpuRecyclableResourceProvider::RasterInterface() const {
  if (!ContextProviderWrapper()) {
    return nullptr;
  }
  return ContextProviderWrapper()->ContextProvider().RasterInterface();
}

bool WebGpuRecyclableResourceProvider::IsGpuContextLost() const {
  auto* raster_interface = RasterInterface();
  return !raster_interface ||
         raster_interface->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

void WebGpuRecyclableResourceProvider::PrepareForWebGPUDummyMailbox() {
  if (resource()) {
    resource()->PrepareForWebGPUDummyMailbox();
  }
}

scoped_refptr<gpu::ClientSharedImage>
WebGpuRecyclableResourceProvider::BeginExternalOverwrite(
    gpu::SyncToken& internal_access_sync_token) {
  if (IsGpuContextLost()) {
    return nullptr;
  }

  // End the internal write access before calling WillDrawInternal(), which
  // has a precondition that there should be no current write access on the
  // resource.
  EndWriteAccess();

  // NOTE: Invoking WillDrawInternal() ensures that this invocation of
  // EndAccess() will generate a new sync token.
  auto access = WillDrawInternal();
  resource_->EndAccess(std::move(access));
  internal_access_sync_token = resource_->sync_token();
  return resource_->GetSharedImage();
}

void WebGpuRecyclableResourceProvider::EndExternalWrite(
    const gpu::SyncToken& external_write_sync_token) {
  if (IsGpuContextLost()) {
    return;
  }

  resource()->EndExternalWrite(external_write_sync_token);
}

void WebGpuRecyclableResourceProvider::DoExternalOverdraw(
    base::FunctionRef<void(cc::PaintCanvas&)> draw_callback) {
  if (IsGpuContextLost()) {
    return;
  }

  draw_callback(recorder_for_external_draws_->getRecordingCanvas());
  if (recorder_for_external_draws_->HasReleasableDrawOps()) {
    cc::PaintRecord last_recording =
        recorder_for_external_draws_->ReleaseMainRecording();

    auto access = WillDrawInternal();
    EnsureWriteAccess();

    const bool needs_clear = !is_cleared_;
    is_cleared_ = true;

    gpu::raster::RasterInterface* ri = RasterInterface();
    SkColor4f background_color = GetAlphaType() == kOpaque_SkAlphaType
                                     ? SkColors::kBlack
                                     : SkColors::kTransparent;

    auto list = base::MakeRefCounted<cc::DisplayItemList>();
    list->StartPaint();
    list->push<cc::DrawRecordOp>(std::move(last_recording));
    list->EndPaintOfUnpaired(gfx::Rect(Size().width(), Size().height()));
    list->Finalize();

    gfx::Size size(Size().width(), Size().height());
    size_t max_op_size_hint =
        gpu::raster::RasterInterface::kDefaultMaxOpSizeHint;
    gfx::Rect full_raster_rect(Size().width(), Size().height());
    gfx::Rect playback_rect(Size().width(), Size().height());
    gfx::Vector2dF post_translate(0.f, 0.f);
    gfx::Vector2dF post_scale(1.f, 1.f);

    const bool can_use_lcd_text = GetAlphaType() == kOpaque_SkAlphaType;
    const auto& caps =
        context_provider_wrapper_->ContextProvider().GetCapabilities();
    bool use_msaa = !caps.msaa_is_slow && !caps.avoid_stencil_buffers;
    ri->BeginRasterCHROMIUM(background_color, needs_clear,
                            /*msaa_sample_count=*/use_msaa ? 1 : 0,
                            use_msaa ? gpu::raster::MsaaMode::kDMSAA
                                     : gpu::raster::MsaaMode::kNoMSAA,
                            can_use_lcd_text, /*visible=*/true, GetColorSpace(),
                            /*hdr_headroom=*/0.f,
                            resource()->GetSharedImage()->mailbox().name);

    auto* image_provider = GetOrCreateImageProvider();
    ri->RasterCHROMIUM(
        list.get(), image_provider, size, full_raster_rect, playback_rect,
        post_translate, post_scale, /*requires_clear=*/false,
        /*raster_inducing_scroll_offsets=*/nullptr, &max_op_size_hint,
        base::RepeatingCallback<void(SkCanvas*, uint32_t)>());

    ri->EndRasterCHROMIUM();
    resource()->EndAccess(std::move(access));

    if (canvas_image_provider_) {
      canvas_image_provider_->ReleaseLockedImages();
      canvas_image_provider_->UnbindTextureBackedImages();
    }
  }

  // We are about to give the caller read access to this resource (and its
  // backing SharedImage). Hence, we must give up the current write access
  // (if any).
  EndWriteAccess();
}

std::unique_ptr<gpu::RasterScopedAccess>
WebGpuRecyclableResourceProvider::WillDrawInternal() {
  DCHECK(resource_);
  return resource_->BeginAccess(/*readonly=*/false);
}

bool WebGpuRecyclableResourceProvider::UploadToBackingSharedImage(
    const SkPixmap& pixmap,
    uint32_t src_x,
    uint32_t src_y) {
  const int dest_width = Size().width();
  const int dest_height = Size().height();

  SkPixmap subset;
  if (!pixmap.extractSubset(
          &subset,
          SkIRect::MakeXYWH(static_cast<int>(src_x), static_cast<int>(src_y),
                            dest_width, dest_height))) {
    return false;
  }

  TRACE_EVENT0("blink",
               "WebGpuRecyclableResourceProvider::"
               "UploadToBackingSharedImage");
  if (IsGpuContextLost()) {
    return false;
  }

  auto access = WillDrawInternal();

  auto client_si = resource()->GetSharedImage();
  RasterInterface()->WritePixels(client_si->mailbox(), /*dst_x_offset=*/0,
                                 /*dst_y_offset=*/0,
                                 client_si->GetTextureTarget(), subset);
  resource()->EndAccess(std::move(access));

  is_cleared_ = true;

  return true;
}

bool WebGpuRecyclableResourceProvider::CopyToBackingSharedImage(
    const scoped_refptr<gpu::ClientSharedImage>& shared_image,
    uint32_t src_x,
    uint32_t src_y,
    const gpu::SyncToken& ready_sync_token,
    gpu::SyncToken& completion_sync_token) {
  gpu::raster::RasterInterface* raster = RasterInterface();
  if (!raster) {
    return false;
  }

  if (IsGpuContextLost()) {
    return false;
  }

  gfx::Rect copy_rect(src_x, src_y, Size().width(), Size().height());

  EndWriteAccess();
  auto dst_access = WillDrawInternal();

  auto dst_client_si = resource()->GetSharedImage();
  if (!dst_client_si) {
    resource()->EndAccess(std::move(dst_access));
    return false;
  }

  std::unique_ptr<gpu::RasterScopedAccess> src_access =
      shared_image->BeginRasterAccess(raster, ready_sync_token,
                                      /*readonly=*/true);
  raster->CopySharedImage(shared_image->mailbox(), dst_client_si->mailbox(),
                          /*xoffset=*/0,
                          /*yoffset=*/0, copy_rect.x(), copy_rect.y(),
                          copy_rect.width(), copy_rect.height());
  completion_sync_token =
      gpu::RasterScopedAccess::EndAccess(std::move(src_access));
  resource()->EndAccess(std::move(dst_access));
  is_cleared_ = true;
  return true;
}

scoped_refptr<gpu::ClientSharedImage>
WebGpuRecyclableResourceProvider::GetSharedImage() const {
  if (IsGpuContextLost()) {
    return nullptr;
  }
  return resource_ ? resource_->GetSharedImage() : nullptr;
}

gpu::SyncToken WebGpuRecyclableResourceProvider::GetSyncToken() const {
  if (IsGpuContextLost()) {
    return gpu::SyncToken();
  }
  return resource_ ? resource_->sync_token() : gpu::SyncToken();
}

CanvasImageProvider*
WebGpuRecyclableResourceProvider::GetOrCreateImageProvider() {
  if (!canvas_image_provider_) {
    if (!IsGpuContextLost()) {
      // Create an ImageDecodeCache for half float images only if the canvas
      // is using half float back storage.
      cc::ImageDecodeCache* cache_f16 = nullptr;
      if (GetSharedImageFormat() == viz::SinglePlaneFormat::kRGBA_F16) {
        cache_f16 =
            context_provider_wrapper_->ContextProvider().ImageDecodeCache(
                kRGBA_F16_SkColorType);
      }

      cc::ImageDecodeCache* cache_rgba8 =
          context_provider_wrapper_->ContextProvider().ImageDecodeCache(
              kN32_SkColorType);

      canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
          cache_rgba8, cache_f16, GetColorSpace(), GetSharedImageFormat(),
          cc::PlaybackImageProvider::RasterMode::kGpu);
    }
  }
  return canvas_image_provider_.get();
}



base::ByteSize WebGpuRecyclableResourceProvider::EstimatedSizeInBytes() const {
  base::ByteSize result;
  if (resource_) {
    result += resource_->EstimatedSizeInBytes() * num_inflight_resources_;
  }
  return result;
}

void WebGpuRecyclableResourceProvider::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  std::string path = base::StringPrintf("canvas/ResourceProvider_0x%" PRIXPTR,
                                        reinterpret_cast<uintptr_t>(this));

  resource()->OnMemoryDump(pmd, path);
}

size_t WebGpuRecyclableResourceProvider::GetSize() const {
  return base::checked_cast<size_t>(EstimatedSizeInBytes().InBytes());
}

}  // namespace blink
