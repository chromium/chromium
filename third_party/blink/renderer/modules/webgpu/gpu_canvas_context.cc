// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlcanvaselement_offscreencanvas.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_alpha_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasrenderingcontext2d_gpucanvascontext_imagebitmaprenderingcontext_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpucanvascontext_imagebitmaprenderingcontext_offscreencanvasrenderingcontext2d_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_texture_alpha_clearer.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

GPUCanvasContext::Factory::~Factory() = default;

CanvasRenderingContext* GPUCanvasContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  CanvasRenderingContext* rendering_context =
      MakeGarbageCollected<GPUCanvasContext>(host, attrs);
  DCHECK(host);
  return rendering_context;
}

CanvasRenderingContext::CanvasRenderingAPI
GPUCanvasContext::Factory::GetRenderingAPI() const {
  return CanvasRenderingContext::CanvasRenderingAPI::kWebgpu;
}

GPUCanvasContext::GPUCanvasContext(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs)
    : CanvasRenderingContext(host, attrs, CanvasRenderingAPI::kWebgpu) {
  // Set the default values of the member corresponding to
  // GPUCanvasContext.[[texture_descriptor]] in the WebGPU spec.
  texture_descriptor_ = {};
  texture_descriptor_.dimension = WGPUTextureDimension_2D;
  texture_descriptor_.mipLevelCount = 1;
  texture_descriptor_.sampleCount = 1;
}

GPUCanvasContext::~GPUCanvasContext() {
  // Perform destruction that's safe to do inside a GC (as in it doesn't touch
  // other GC objects).
  if (swap_buffers_) {
    swap_buffers_->Neuter();
  }
}

void GPUCanvasContext::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(texture_);
  CanvasRenderingContext::Trace(visitor);
}

// CanvasRenderingContext implementation
V8RenderingContext* GPUCanvasContext::AsV8RenderingContext() {
  return MakeGarbageCollected<V8RenderingContext>(this);
}

V8OffscreenRenderingContext* GPUCanvasContext::AsV8OffscreenRenderingContext() {
  return MakeGarbageCollected<V8OffscreenRenderingContext>(this);
}

SkColorInfo GPUCanvasContext::CanvasRenderingContextSkColorInfo() const {
  if (!swap_buffers_)
    return CanvasRenderingContext::CanvasRenderingContextSkColorInfo();
  return SkColorInfo(viz::ResourceFormatToClosestSkColorType(
                         /*gpu_compositing=*/true, swap_buffers_->Format()),
                     alpha_mode_ == V8GPUCanvasAlphaMode::Enum::kOpaque
                         ? kOpaque_SkAlphaType
                         : kPremul_SkAlphaType,
                     PredefinedColorSpaceToSkColorSpace(color_space_));
}

void GPUCanvasContext::Stop() {
  DetachSwapBuffers();
  stopped_ = true;
}

cc::Layer* GPUCanvasContext::CcLayer() const {
  if (swap_buffers_) {
    return swap_buffers_->CcLayer();
  }
  return nullptr;
}

void GPUCanvasContext::Reshape(int width, int height) {
  if (stopped_) {
    return;
  }

  ResizeSwapbuffers(gfx::Size(width, height));
}

scoped_refptr<StaticBitmapImage> GPUCanvasContext::GetImage() {
  if (!swap_buffers_)
    return nullptr;

  // If there is a current texture, create a snapshot from it.
  if (texture_) {
    return SnapshotInternal(texture_->GetHandle(), swap_buffers_->Size());
  }

  // If there is no current texture, we need to get the information of the last
  // texture reserved, that contains the last mailbox, create a new texture for
  // it, and use it to create the resource provider. We also need the size of
  // the texture to create the resource provider.
  auto mailbox_texture_size =
      swap_buffers_->GetLastWebGPUMailboxTextureAndSize();
  if (!mailbox_texture_size.mailbox_texture)
    return nullptr;
  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      mailbox_texture_size.mailbox_texture;
  gfx::Size size = mailbox_texture_size.size;

  return SnapshotInternal(mailbox_texture->GetTexture(), size);
}

bool GPUCanvasContext::PaintRenderingResultsToCanvas(
    SourceDrawingBuffer source_buffer) {
  if (!swap_buffers_)
    return false;

  if (Host()->ResourceProvider() &&
      Host()->ResourceProvider()->Size() != swap_buffers_->Size()) {
    Host()->DiscardResourceProvider();
  }

  CanvasResourceProvider* resource_provider =
      Host()->GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);

  return CopyRenderingResultsFromDrawingBuffer(resource_provider,
                                               source_buffer);
}

bool GPUCanvasContext::CopyRenderingResultsFromDrawingBuffer(
    CanvasResourceProvider* resource_provider,
    SourceDrawingBuffer source_buffer) {
  // TODO(crbug.com/1367056): Handle source_buffer == kFrontBuffer.
  // By returning false here the canvas will show up as black in the scenarios
  // that copy the front buffer, such as printing.
  if (source_buffer != kBackBuffer) {
    return false;
  }

  if (!texture_)
    return false;

  return CopyTextureToResourceProvider(
      texture_->GetHandle(), swap_buffers_->Size(), resource_provider);
}

bool GPUCanvasContext::CopyRenderingResultsToVideoFrame(
    WebGraphicsContext3DVideoFramePool* frame_pool,
    SourceDrawingBuffer src_buffer,
    const gfx::ColorSpace& dst_color_space,
    VideoFrameCopyCompletedCallback callback) {
  if (!swap_buffers_) {
    return false;
  }

  return swap_buffers_->CopyToVideoFrame(frame_pool, src_buffer,
                                         dst_color_space, std::move(callback));
}

void GPUCanvasContext::SetFilterQuality(
    cc::PaintFlags::FilterQuality filter_quality) {
  if (filter_quality != filter_quality_) {
    filter_quality_ = filter_quality;
    if (swap_buffers_) {
      swap_buffers_->SetFilterQuality(filter_quality);
    }
  }
}

bool GPUCanvasContext::PushFrame() {
  DCHECK(Host());
  DCHECK(Host()->IsOffscreenCanvas());

  if (!swap_buffers_)
    return false;

  viz::TransferableResource transferable_resource;
  viz::ReleaseCallback release_callback;
  if (!swap_buffers_->PrepareTransferableResource(
          nullptr, &transferable_resource, &release_callback)) {
    return false;
  }

  // Acquires a CanvasResource of type ExternalCanvasResource that will
  // encapsulate an external mailbox, synctoken and release callback.
  SkImageInfo resource_info = SkImageInfo::Make(
      transferable_resource.size.width(), transferable_resource.size.height(),
      viz::ToClosestSkColorType(
          /*gpu_compositing=*/true, transferable_resource.format),
      kPremul_SkAlphaType);
  auto canvas_resource = ExternalCanvasResource::Create(
      transferable_resource.mailbox_holder.mailbox, std::move(release_callback),
      transferable_resource.mailbox_holder.sync_token, resource_info,
      transferable_resource.mailbox_holder.texture_target,
      GetContextProviderWeakPtr(), /*resource_provider=*/nullptr,
      cc::PaintFlags::FilterQuality::kLow,
      /*is_origin_top_left=*/kBottomLeft_GrSurfaceOrigin,
      transferable_resource.is_overlay_candidate);
  if (!canvas_resource)
    return false;

  const int width = canvas_resource->Size().width();
  const int height = canvas_resource->Size().height();
  return Host()->PushFrame(std::move(canvas_resource),
                           SkIRect::MakeWH(width, height));
}

ImageBitmap* GPUCanvasContext::TransferToImageBitmap(
    ScriptState* script_state) {
  auto MakeFallbackImageBitmap =
      [this](V8GPUCanvasAlphaMode::Enum alpha_mode) -> ImageBitmap* {
    // It is not possible to create an empty image bitmap, return null in that
    // case which will fail ImageBitmap creation with an exception instead.
    gfx::Size size = Host()->Size();
    if (size.IsEmpty()) {
      return nullptr;
    }

    // We intentionally leave the image in legacy color space.
    SkBitmap black_bitmap;
    black_bitmap.allocN32Pixels(size.width(), size.height());
    if (alpha_mode == V8GPUCanvasAlphaMode::Enum::kOpaque) {
      black_bitmap.eraseARGB(255, 0, 0, 0);
    } else {
      black_bitmap.eraseARGB(0, 0, 0, 0);
    }

    return MakeGarbageCollected<ImageBitmap>(
        UnacceleratedStaticBitmapImage::Create(
            SkImage::MakeFromBitmap(black_bitmap)));
  };

  // If the canvas configuration is invalid, WebGPU requires that we give a
  // fallback black ImageBitmap if possible.
  if (!swap_buffers_) {
    return MakeFallbackImageBitmap(V8GPUCanvasAlphaMode::Enum::kOpaque);
  }

  viz::TransferableResource transferable_resource;
  viz::ReleaseCallback release_callback;
  if (!swap_buffers_->PrepareTransferableResource(
          nullptr, &transferable_resource, &release_callback)) {
    // If we can't get a mailbox, return an transparent black ImageBitmap.
    // The only situation in which this could happen is when two or more calls
    // to transferToImageBitmap are made back-to-back, or when the context gets
    // lost.
    return MakeFallbackImageBitmap(alpha_mode_);
  }
  DCHECK(release_callback);

  // We reuse the same mailbox name from above since our texture id was consumed
  // from it.
  const auto& sk_image_mailbox = transferable_resource.mailbox_holder.mailbox;
  // Use the sync token generated after producing the mailbox. Waiting for this
  // before trying to use the mailbox with some other context will ensure it is
  // valid.
  const auto& sk_image_sync_token =
      transferable_resource.mailbox_holder.sync_token;

  auto sk_color_type = viz::ToClosestSkColorType(
      /*gpu_compositing=*/true, transferable_resource.format);

  const SkImageInfo sk_image_info = SkImageInfo::Make(
      texture_descriptor_.size.width, texture_descriptor_.size.height,
      sk_color_type, kPremul_SkAlphaType);

  return MakeGarbageCollected<ImageBitmap>(
      AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
          sk_image_mailbox, sk_image_sync_token,
          /* shared_image_texture_id = */ 0, sk_image_info,
          transferable_resource.mailbox_holder.texture_target,
          /* is_origin_top_left = */ kBottomLeft_GrSurfaceOrigin,
          GetContextProviderWeakPtr(), base::PlatformThread::CurrentRef(),
          ThreadScheduler::Current()->CleanupTaskRunner(),
          std::move(release_callback),
          /*supports_display_compositing=*/true,
          transferable_resource.is_overlay_candidate));
}

// gpu_presentation_context.idl
V8UnionHTMLCanvasElementOrOffscreenCanvas*
GPUCanvasContext::getHTMLOrOffscreenCanvas() const {
  if (Host()->IsOffscreenCanvas()) {
    return MakeGarbageCollected<V8UnionHTMLCanvasElementOrOffscreenCanvas>(
        static_cast<OffscreenCanvas*>(Host()));
  }
  return MakeGarbageCollected<V8UnionHTMLCanvasElementOrOffscreenCanvas>(
      static_cast<HTMLCanvasElement*>(Host()));
}

void GPUCanvasContext::configure(const GPUCanvasConfiguration* descriptor,
                                 ExceptionState& exception_state) {
  DCHECK(descriptor);

  if (stopped_ || !Host()) {
    // This is probably not possible, or at least would only happen during page
    // shutdown.
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "canvas has been destroyed");
    return;
  }

  if (!descriptor->device()->ValidateTextureFormatUsage(descriptor->format(),
                                                        exception_state)) {
    return;
  }

  for (auto view_format : descriptor->viewFormats()) {
    if (!descriptor->device()->ValidateTextureFormatUsage(view_format,
                                                          exception_state)) {
      return;
    }
  }

  // As soon as the validation for extensions for usage and formats passes, the
  // canvas is "configured" and calls to getNextTexture() will return GPUTexture
  // objects (valid or invalid) and not throw.
  configured_ = true;
  texture_descriptor_.format = AsDawnEnum(descriptor->format());
  texture_descriptor_.usage =
      AsDawnFlags<WGPUTextureUsage>(descriptor->usage());

  view_formats_ = AsDawnEnum<WGPUTextureFormat>(descriptor->viewFormats());
  texture_descriptor_.viewFormats = view_formats_.get();
  texture_descriptor_.viewFormatCount = descriptor->viewFormats().size();

  // Set the size of the texture in case there was no Reshape() since the
  // creation of the context.
  gfx::Size host_size = Host()->Size();
  texture_descriptor_.size = {static_cast<uint32_t>(host_size.width()),
                              static_cast<uint32_t>(host_size.height()), 1};

  // This needs to happen early so that if any validation fails the swapbuffers
  // are not created and getCurrentTexture() will return an error GPUTexture.
  DetachSwapBuffers();

  // Store the configured device separately, even if the configuration fails, so
  // that errors can be generated in the appropriate error scope.
  device_ = descriptor->device();

  // The WebGPU spec requires that a validation error be produced if the
  // descriptor is invalid. However no call to AssociateMailbox is done in
  // configure() which would produce the error. Directly request that the
  // descriptor be validated instead.
  device_->GetProcs().deviceValidateTextureDescriptor(device_->GetHandle(),
                                                      &texture_descriptor_);

  switch (texture_descriptor_.format) {
    // TODO(crbug.com/1361468): support BGRA8Unorm on Android.
#if !BUILDFLAG(IS_ANDROID)
    case WGPUTextureFormat_BGRA8Unorm:
#endif
      // TODO(crbug.com/1298618): support RGBA8Unorm on MAC.
#if !BUILDFLAG(IS_MAC)
    case WGPUTextureFormat_RGBA8Unorm:
#endif
      // TODO(crbug.com/1317015): support RGBA16Float on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
    case WGPUTextureFormat_RGBA16Float:
#endif
      break;
    default:
      device_->InjectError(
          WGPUErrorType_Validation,
          ("unsupported swap chain format \"" +
           std::string(FromDawnEnum(texture_descriptor_.format)) + "\"")
              .c_str());
      return;
  }

  alpha_mode_ = descriptor->alphaMode().AsEnum();

  if (!ValidateAndConvertColorSpace(descriptor->colorSpace(), color_space_,
                                    exception_state)) {
    return;
  }

  swap_buffers_ = base::AdoptRef(new WebGPUSwapBufferProvider(
      this, device_->GetDawnControlClient(), device_->GetHandle(),
      static_cast<WGPUTextureUsage>(texture_descriptor_.usage),
      texture_descriptor_.format, color_space_));
  swap_buffers_->SetFilterQuality(filter_quality_);

  // Note: SetContentsOpaque is only an optimization hint. It doesn't
  // actually make the contents opaque.
  switch (alpha_mode_) {
    case V8GPUCanvasAlphaMode::Enum::kOpaque: {
      CcLayer()->SetContentsOpaque(true);
      if (!alpha_clearer_ ||
          !alpha_clearer_->IsCompatible(device_->GetHandle(),
                                        texture_descriptor_.format)) {
        alpha_clearer_ = base::MakeRefCounted<WebGPUTextureAlphaClearer>(
            device_->GetDawnControlClient(), device_->GetHandle(),
            texture_descriptor_.format);
      }
      break;
    }
    case V8GPUCanvasAlphaMode::Enum::kPremultiplied:
      alpha_clearer_ = nullptr;
      CcLayer()->SetContentsOpaque(false);
      break;
  }

  ResizeSwapbuffers(host_size);
}

void GPUCanvasContext::ResizeSwapbuffers(gfx::Size size) {
  texture_descriptor_.size = {static_cast<uint32_t>(size.width()),
                              static_cast<uint32_t>(size.height()), 1};

  // The spec indicates that when the canvas is resized the current texture is
  // discarded and a new one allocated in it's place immediately.
  if (swap_buffers_) {
    ReplaceCurrentTexture();
  }

  // If we don't notify the host that something has changed it may never check
  // for the new cc::Layer.
  Host()->SetNeedsCompositingUpdate();
}

void GPUCanvasContext::unconfigure() {
  if (stopped_) {
    return;
  }

  DetachSwapBuffers();

  // When developers call unconfigure from the page, one of the reasons for
  // doing so is to expressly release the GPUCanvasContext's device reference.
  // In order to fully release it, any TextureAlphaClearer that has been created
  // also needs to be released.
  alpha_clearer_ = nullptr;
  device_ = nullptr;
  configured_ = false;
}

void GPUCanvasContext::DetachSwapBuffers() {
  if (swap_buffers_) {
    // Tell any previous swapbuffers that it will no longer be used and can
    // destroy all its resources (and produce errors when used).
    swap_buffers_->Neuter();
    swap_buffers_ = nullptr;
  }
  texture_ = nullptr;
}

GPUTexture* GPUCanvasContext::getCurrentTexture(
    ExceptionState& exception_state) {
  if (!configured_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "context is not configured");
    return nullptr;
  }
  DCHECK(device_);

  if (!swap_buffers_) {
    device_->InjectError(WGPUErrorType_Validation,
                         "context configuration is invalid.");
    return GPUTexture::CreateError(device_, &texture_descriptor_);
  }

  // Calling getCurrentTexture returns a texture that is valid until the
  // animation frame it gets presented. If getCurrentTexture is called multiple
  // time, the same texture should be returned. |texture_| is set to null when
  // presented so that we know we should create a new one.
  if (texture_ && !new_texture_required_) {
    return texture_;
  }

  return ReplaceCurrentTexture();
}

GPUTexture* GPUCanvasContext::ReplaceCurrentTexture() {
  DCHECK(device_);
  DCHECK(swap_buffers_);

  // Simply requesting a new canvas texture with WebGPU is enough to mark it as
  // "dirty", so always call DidDraw() when a new texture is created.
  DidDraw(CanvasPerformanceMonitor::DrawType::kOther);

  if (texture_) {
    swap_buffers_->DiscardCurrentSwapBuffer();
  }

  texture_ = nullptr;

  SkAlphaType alpha_type = alpha_mode_ == V8GPUCanvasAlphaMode::Enum::kOpaque
                               ? kOpaque_SkAlphaType
                               : kPremul_SkAlphaType;
  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      swap_buffers_->GetNewTexture(texture_descriptor_, alpha_type);
  if (!mailbox_texture) {
    texture_ = GPUTexture::CreateError(device_, &texture_descriptor_);
    return texture_;
  }

  mailbox_texture->SetNeedsPresent(true);
  mailbox_texture->SetAlphaClearer(alpha_clearer_);

  texture_ = MakeGarbageCollected<GPUTexture>(
      device_, texture_descriptor_.format,
      static_cast<WGPUTextureUsage>(texture_descriptor_.usage),
      std::move(mailbox_texture));
  new_texture_required_ = false;

  return texture_;
}

void GPUCanvasContext::FinalizeFrame(bool /*printing*/) {
  // In some cases, such as when a canvas is hidden of offscreen, compositing
  // will never happen and thus OnTextureTransferred will never be called. In
  // those cases, getCurrentTexture is still required to return a new texture
  // after the current frame has ended, so we'll mark that a new texture is
  // required here.
  new_texture_required_ = true;
}

// WebGPUSwapBufferProvider::Client implementation
void GPUCanvasContext::OnTextureTransferred() {
  DCHECK(texture_);
  texture_ = nullptr;
}

bool GPUCanvasContext::CopyTextureToResourceProvider(
    const WGPUTexture& texture,
    const gfx::Size& size,
    CanvasResourceProvider* resource_provider) const {
  DCHECK(resource_provider);
  DCHECK_EQ(resource_provider->Size(), size);
  DCHECK(resource_provider->GetSharedImageUsageFlags() &
         gpu::SHARED_IMAGE_USAGE_WEBGPU);
  DCHECK(resource_provider->IsOriginTopLeft());

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> shared_context_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  if (!shared_context_wrapper || !shared_context_wrapper->ContextProvider())
    return false;

  const auto dst_mailbox =
      resource_provider->GetBackingMailboxForOverwrite(kUnverifiedSyncToken);
  if (dst_mailbox.IsZero())
    return false;

  auto* ri = shared_context_wrapper->ContextProvider()->RasterInterface();

  if (!GetContextProviderWeakPtr()) {
    return false;
  }
  // todo(crbug/1267244) Use WebGPUMailboxTexture here instead of doing things
  // manually.
  gpu::webgpu::WebGPUInterface* webgpu =
      GetContextProviderWeakPtr()->ContextProvider()->WebGPUInterface();
  gpu::webgpu::ReservedTexture reservation =
      webgpu->ReserveTexture(device_->GetHandle());
  DCHECK(reservation.texture);

  gpu::SyncToken sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  webgpu->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  webgpu->AssociateMailbox(
      reservation.deviceId, reservation.deviceGeneration, reservation.id,
      reservation.generation,
      WGPUTextureUsage_CopyDst | WGPUTextureUsage_RenderAttachment,
      dst_mailbox);
  WGPUImageCopyTexture source = {
      .nextInChain = nullptr,
      .texture = texture,
      .mipLevel = 0,
      .origin = WGPUOrigin3D{0},
      .aspect = WGPUTextureAspect_All,
  };
  WGPUImageCopyTexture destination = {
      .nextInChain = nullptr,
      .texture = reservation.texture,
      .mipLevel = 0,
      .origin = WGPUOrigin3D{0},
      .aspect = WGPUTextureAspect_All,
  };
  WGPUExtent3D copy_size = {
      .width = static_cast<uint32_t>(size.width()),
      .height = static_cast<uint32_t>(size.height()),
      .depthOrArrayLayers = 1,
  };

  if (alpha_mode_ == V8GPUCanvasAlphaMode::Enum::kOpaque) {
    // Issue a copyTextureForBrowser call with internal usage turned on.
    // There is a special step for srcAlphaMode == WGPUAlphaMode_Opaque that
    // clears alpha channel to one.
    SkImageInfo sk_dst_image_info = resource_provider->GetSkImageInfo();
    WGPUAlphaMode dstAlphaMode;
    switch (sk_dst_image_info.alphaType()) {
      case SkAlphaType::kPremul_SkAlphaType:
        dstAlphaMode = WGPUAlphaMode_Premultiplied;
        break;
      case SkAlphaType::kUnpremul_SkAlphaType:
        dstAlphaMode = WGPUAlphaMode_Unpremultiplied;
        break;
      case SkAlphaType::kOpaque_SkAlphaType:
        dstAlphaMode = WGPUAlphaMode_Opaque;
        break;
      default:
        // Unknown dst alpha type, default to equal to src alpha mode
        dstAlphaMode = WGPUAlphaMode_Opaque;
        break;
    }
    WGPUCopyTextureForBrowserOptions options = {
        .flipY = !resource_provider->IsOriginTopLeft(),
        .srcAlphaMode = WGPUAlphaMode_Opaque,
        .dstAlphaMode = dstAlphaMode,
        .internalUsage = true,
    };

    GetProcs().queueCopyTextureForBrowser(device_->queue()->GetHandle(),
                                          &source, &destination, &copy_size,
                                          &options);

  } else {
    // Create a command encoder and call copyTextureToTexture for the image
    // copy.
    WGPUDawnEncoderInternalUsageDescriptor internal_usage_desc = {
        .chain = {.sType = WGPUSType_DawnEncoderInternalUsageDescriptor},
        .useInternalUsages = true,
    };
    WGPUCommandEncoderDescriptor command_encoder_desc = {
        .nextInChain = &internal_usage_desc.chain,
    };
    WGPUCommandEncoder command_encoder = GetProcs().deviceCreateCommandEncoder(
        device_->GetHandle(), &command_encoder_desc);
    GetProcs().commandEncoderCopyTextureToTexture(command_encoder, &source,
                                                  &destination, &copy_size);

    WGPUCommandBuffer command_buffer =
        GetProcs().commandEncoderFinish(command_encoder, nullptr);
    GetProcs().commandEncoderRelease(command_encoder);

    GetProcs().queueSubmit(device_->queue()->GetHandle(), 1u, &command_buffer);
    GetProcs().commandBufferRelease(command_buffer);
  }

  webgpu->DissociateMailbox(reservation.id, reservation.generation);
  GetProcs().textureRelease(reservation.texture);
  webgpu->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());

  return true;
}

scoped_refptr<StaticBitmapImage> GPUCanvasContext::SnapshotInternal(
    const WGPUTexture& texture,
    const gfx::Size& size) const {
  const auto canvas_context_color = CanvasRenderingContextSkColorInfo();
  const auto info = SkImageInfo::Make(size.width(), size.height(),
                                      canvas_context_color.colorType(),
                                      canvas_context_color.alphaType());
  // We tag the SharedImage inside the WebGPUImageProvider with display usage
  // since there are uncommon paths which may use this snapshot for compositing.
  // These paths are usually related to either printing or either video and
  // usually related to OffscreenCanvas; in cases where the image created from
  // this Snapshot will be sent eventually to the Display Compositor.
  auto resource_provider = CanvasResourceProvider::CreateWebGPUImageProvider(
      info,
      /*is_origin_top_left=*/true, gpu::SHARED_IMAGE_USAGE_DISPLAY_READ);
  if (!resource_provider)
    return nullptr;

  if (!CopyTextureToResourceProvider(texture, size, resource_provider.get()))
    return nullptr;

  return resource_provider->Snapshot();
}

// DawnObjectBase substitute methods
const DawnProcTable& GPUCanvasContext::GetProcs() const {
  return device_->GetProcs();
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
GPUCanvasContext::GetContextProviderWeakPtr() const {
  return device_->GetDawnControlClient()->GetContextProviderWeakPtr();
}

}  // namespace blink
