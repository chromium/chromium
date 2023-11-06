// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"

#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlcanvaselement_offscreencanvas.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_alpha_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasrenderingcontext2d_gpucanvascontext_imagebitmaprenderingcontext_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpucanvascontext_imagebitmaprenderingcontext_offscreencanvasrenderingcontext2d_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_texture_alpha_clearer.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/skia/include/core/SkImage.h"

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
  texture_descriptor_ = {};
}

GPUCanvasContext::~GPUCanvasContext() {
  copy_to_swap_texture_required_ = false;

  // Perform destruction that's safe to do inside a GC (as in it doesn't touch
  // other GC objects).
  if (swap_buffers_) {
    swap_buffers_->Neuter();
  }
}

void GPUCanvasContext::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(texture_);
  visitor->Trace(swap_texture_);
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
  return SkColorInfo(viz::ToClosestSkColorType(
                         /*gpu_compositing=*/true, swap_buffers_->Format()),
                     alpha_mode_ == V8GPUCanvasAlphaMode::Enum::kOpaque
                         ? kOpaque_SkAlphaType
                         : kPremul_SkAlphaType,
                     PredefinedColorSpaceToSkColorSpace(color_space_));
}

void GPUCanvasContext::Stop() {
  ReplaceDrawingBuffer(/*destroy_swap_buffers*/ true);
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

  // Steps for canvas context resizing:
  // 1. Replace the drawing buffer of context.
  ReplaceDrawingBuffer(/* destroy_swap_buffers */ false);

  // 2. Let configuration be context.[[configuration]]
  // 3. If configuration is not null:
  //   1. Set context.[[textureDescriptor]] to the GPUTextureDescriptor for the
  //      canvas and configuration(canvas, configuration).
  texture_descriptor_.size = {static_cast<uint32_t>(width),
                              static_cast<uint32_t>(height), 1};

  swap_texture_descriptor_.size = texture_descriptor_.size;

  // If we don't notify the host that something has changed it may never check
  // for the new cc::Layer.
  Host()->SetNeedsCompositingUpdate();
}

scoped_refptr<StaticBitmapImage> GPUCanvasContext::GetImage(FlushReason) {
  if (!swap_buffers_)
    return nullptr;

  // If there is a current texture, create a snapshot from it.
  if (texture_ && !texture_->Destroyed()) {
    return SnapshotInternal(texture_->GetHandle(), swap_buffers_->Size());
  } else if (swap_texture_) {
    return SnapshotInternal(swap_texture_->GetHandle(), swap_buffers_->Size());
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

  if (!texture_) {
    return false;
  }

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

  auto canvas_resource = ExternalCanvasResource::Create(
      transferable_resource, std::move(release_callback),
      GetContextProviderWeakPtr(), /*resource_provider=*/nullptr,
      cc::PaintFlags::FilterQuality::kLow,
      /*is_origin_top_left=*/kBottomLeft_GrSurfaceOrigin);
  if (!canvas_resource)
    return false;

  const int width = canvas_resource->Size().width();
  const int height = canvas_resource->Size().height();
  return Host()->PushFrame(std::move(canvas_resource),
                           SkIRect::MakeWH(width, height));
}

ImageBitmap* GPUCanvasContext::TransferToImageBitmap(
    ScriptState* script_state,
    ExceptionState& exception_state) {
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
    if (!black_bitmap.tryAllocN32Pixels(size.width(), size.height())) {
      // It is not possible to create such a big image bitmap, return null in
      // that case which will fail ImageBitmap creation with an exception
      // instead.
      return nullptr;
    }

    if (alpha_mode == V8GPUCanvasAlphaMode::Enum::kOpaque) {
      black_bitmap.eraseARGB(255, 0, 0, 0);
    } else {
      black_bitmap.eraseARGB(0, 0, 0, 0);
    }

    return MakeGarbageCollected<ImageBitmap>(
        UnacceleratedStaticBitmapImage::Create(
            SkImages::RasterFromBitmap(black_bitmap)));
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

  // Set the default values of the member corresponding to
  // GPUCanvasContext.[[texture_descriptor]] in the WebGPU spec.
  texture_descriptor_ = {};
  texture_descriptor_.dimension = WGPUTextureDimension_2D;
  texture_descriptor_.mipLevelCount = 1;
  texture_descriptor_.sampleCount = 1;

  // Set the values from the configuration descriptor
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

  // Reconfiguring the context discards previous drawing buffers but we also
  // destroy the swap buffers so that any validation error below will cause
  // swap_buffers_ to be nullptr and getCurrentTexture() to fail.
  ReplaceDrawingBuffer(/*destroy_swap_buffers*/ true);

  // Store the configured device separately, even if the configuration fails, so
  // that errors can be generated in the appropriate error scope.
  device_ = descriptor->device();

  // The WebGPU spec requires that a validation error be produced if the
  // descriptor is invalid. However no call to AssociateMailbox is done in
  // configure() which would produce the error. Directly request that the
  // descriptor be validated instead.
  device_->GetProcs().deviceValidateTextureDescriptor(device_->GetHandle(),
                                                      &texture_descriptor_);

  copy_to_swap_texture_required_ = false;
  switch (texture_descriptor_.format) {
    case WGPUTextureFormat_BGRA8Unorm:
#if BUILDFLAG(IS_ANDROID)
      // BGRA8Unorm is not natively supported by Android's compositor.
      copy_to_swap_texture_required_ = true;
#endif
      break;

    case WGPUTextureFormat_RGBA8Unorm:
#if BUILDFLAG(IS_MAC)
      // RGBA8Unorm is not natively supported by MacOS's compositor.
      copy_to_swap_texture_required_ = true;
#endif
      break;

    case WGPUTextureFormat_RGBA16Float:
#if BUILDFLAG(IS_CHROMEOS)
      // TODO(crbug.com/1317015): support RGBA16Float on ChromeOS.
      device_->InjectError(
          WGPUErrorType_Validation,
          ("Support for canvas context format \"" +
           std::string(FromDawnEnum(texture_descriptor_.format)) +
           "\" has not been implemented.")
              .c_str());
      return;
#else
      break;
#endif

    default:
      device_->InjectError(
          WGPUErrorType_Validation,
          ("Unsupported canvas context format \"" +
           std::string(FromDawnEnum(texture_descriptor_.format)) + "\"")
              .c_str());
      return;
  }

  // If the context is configured with STORAGE_BINDING texture usage and
  // "bgra8unorm" is the preferred format but the adapter doesn't support the
  // "bgra8unorm-storage" feature, we can guess that the app is using the
  // non-preferred texture format here because it's the only way to get a
  // storage-compatible canvas texture. As such, we'll suppress the warning
  // about not using the preferred format.
  suppress_preferred_format_warning_ = false;
  if (copy_to_swap_texture_required_ &&
      GPU::preferred_canvas_format() == WGPUTextureFormat_BGRA8Unorm &&
      texture_descriptor_.usage & WGPUTextureUsage_StorageBinding &&
      !device_->adapter()->features()->has(
          V8GPUFeatureName::Enum::kBgra8UnormStorage)) {
    suppress_preferred_format_warning_ = true;
  }

  // In the scenario that the system doesn't support the requested format but
  // it's one that WebGPU is required to offer, a separate texture will be
  // returned instead with the desired format and the texture will be copied to
  // the swap buffer provider's texture with the system-supported format when
  // we're ready to present.
  swap_texture_descriptor_ = texture_descriptor_;
  if (copy_to_swap_texture_required_) {
    // The texture returned to the user will require both the CopySrc and
    // TextureBinding usages in order to be used with CopyTextureForBrowser.
    texture_internal_usage_ = {};
    texture_internal_usage_.chain.sType =
        WGPUSType_DawnTextureInternalUsageDescriptor;
    texture_internal_usage_.internalUsage =
        WGPUTextureUsage_CopySrc | WGPUTextureUsage_TextureBinding;
    texture_descriptor_.nextInChain = &texture_internal_usage_.chain;

    // The swap buffer texture will require both CopyDst and RenderAttachment
    // in order to be used with CopyTextureForBrowser.
    swap_texture_descriptor_.usage =
        WGPUTextureUsage_CopyDst | WGPUTextureUsage_RenderAttachment;

    // In cases where a copy is necessary the swap buffers will always use the
    // preferred canvas format.
    swap_texture_descriptor_.format = GPU::preferred_canvas_format();

    // The swap buffer texture doesn't need any view formats.
    swap_texture_descriptor_.viewFormats = nullptr;
    swap_texture_descriptor_.viewFormatCount = 0;
  }

  alpha_mode_ = descriptor->alphaMode().AsEnum();

  if (!ValidateAndConvertColorSpace(descriptor->colorSpace(), color_space_,
                                    exception_state)) {
    return;
  }

  gfx::HDRMetadata hdr_metadata;
  if (descriptor->hasHdrOptions()) {
    ParseCanvasHighDynamicRangeOptions(descriptor->hdrOptions(), hdr_metadata);
  }

  swap_buffers_ = base::AdoptRef(new WebGPUSwapBufferProvider(
      this, device_->GetDawnControlClient(), device_->GetHandle(),
      static_cast<WGPUTextureUsage>(swap_texture_descriptor_.usage),
      swap_texture_descriptor_.format, color_space_, hdr_metadata));
  swap_buffers_->SetFilterQuality(filter_quality_);

  // Note: SetContentsOpaque is only an optimization hint. It doesn't
  // actually make the contents opaque.
  switch (alpha_mode_) {
    case V8GPUCanvasAlphaMode::Enum::kOpaque: {
      CcLayer()->SetContentsOpaque(true);
      if (!alpha_clearer_ ||
          !alpha_clearer_->IsCompatible(device_->GetHandle(),
                                        swap_texture_descriptor_.format)) {
        alpha_clearer_ = base::MakeRefCounted<WebGPUTextureAlphaClearer>(
            device_->GetDawnControlClient(), device_->GetHandle(),
            swap_texture_descriptor_.format);
      }
      break;
    }
    case V8GPUCanvasAlphaMode::Enum::kPremultiplied:
      alpha_clearer_ = nullptr;
      CcLayer()->SetContentsOpaque(false);
      break;
  }

  // If we don't notify the host that something has changed it may never check
  // for the new cc::Layer.
  Host()->SetNeedsCompositingUpdate();
}

void GPUCanvasContext::unconfigure() {
  if (stopped_) {
    return;
  }

  ReplaceDrawingBuffer(/*destroy_swap_buffers*/ true);

  // When developers call unconfigure from the page, one of the reasons for
  // doing so is to expressly release the GPUCanvasContext's device reference.
  // In order to fully release it, any TextureAlphaClearer that has been created
  // also needs to be released.
  alpha_clearer_ = nullptr;
  device_ = nullptr;
  configured_ = false;
}

GPUTexture* GPUCanvasContext::getCurrentTexture(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!configured_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "context is not configured");
    return nullptr;
  }
  DCHECK(device_);

  // Calling getCurrentTexture returns a texture that is valid until the
  // animation frame it gets presented. If getCurrentTexture is called multiple
  // time, the same texture should be returned. |texture_| is set to
  // null when presented so that we know we should create a new one.
  if (texture_ && !new_texture_required_) {
    return texture_.Get();
  }
  new_texture_required_ = false;

  if (!swap_buffers_) {
    device_->InjectError(WGPUErrorType_Validation,
                         "context configuration is invalid.");
    return GPUTexture::CreateError(device_.Get(), &texture_descriptor_);
  }

  ReplaceDrawingBuffer(/* destroy_swap_buffers */ false);

  // Simply requesting a new canvas texture with WebGPU is enough to mark it as
  // "dirty", so always call DidDraw() when a new texture is created.
  DidDraw(CanvasPerformanceMonitor::DrawType::kOther);

  SkAlphaType alpha_type = alpha_mode_ == V8GPUCanvasAlphaMode::Enum::kOpaque
                               ? kOpaque_SkAlphaType
                               : kPremul_SkAlphaType;
  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      swap_buffers_->GetNewTexture(swap_texture_descriptor_, alpha_type);
  if (!mailbox_texture) {
    // Try to give a helpful message for the most common cause for mailbox
    // texture creation failure.
    if (texture_descriptor_.size.width == 0 ||
        texture_descriptor_.size.height == 0) {
      device_->InjectError(WGPUErrorType_Validation,
                           "Could not create a swapchain texture of size 0.");
    } else {
      device_->InjectError(WGPUErrorType_Validation,
                           "Could not create the swapchain texture.");
    }
    texture_ = swap_texture_ =
        GPUTexture::CreateError(device_, &texture_descriptor_);
    return texture_.Get();
  }

  mailbox_texture->SetNeedsPresent(true);
  mailbox_texture->SetAlphaClearer(alpha_clearer_);

  swap_texture_ = MakeGarbageCollected<GPUTexture>(
      device_, swap_texture_descriptor_.format,
      static_cast<WGPUTextureUsage>(swap_texture_descriptor_.usage),
      std::move(mailbox_texture));

  if (copy_to_swap_texture_required_) {
    texture_ = MakeGarbageCollected<GPUTexture>(
        device_, GetProcs().deviceCreateTexture(device_->GetHandle(),
                                                &texture_descriptor_));
    // If the user manually destroys the texture before yielding control back
    // to the browser, do the copy just prior to the texture destruction.
    texture_->SetBeforeDestroyCallback(WTF::BindOnce(
        [](GPUCanvasContext* context, GPUTexture* texture) {
          context->CopyToSwapTexture();
          texture->ClearBeforeDestroyCallback();
        },
        WrapPersistent(this), WrapPersistent(texture_.Get())));
  } else {
    texture_ = swap_texture_;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  UseCounter::Count(execution_context,
                    WebFeature::kWebGPUCanvasContextGetCurrentTexture);

  return texture_.Get();
}

void GPUCanvasContext::ReplaceDrawingBuffer(bool destroy_swap_buffers) {
  if (swap_texture_) {
    DCHECK(swap_buffers_);
    swap_buffers_->DiscardCurrentSwapBuffer();
    swap_texture_ = nullptr;
  }

  if (copy_to_swap_texture_required_ && texture_) {
    texture_->ClearBeforeDestroyCallback();
    texture_->destroy();
  }
  texture_ = nullptr;

  if (swap_buffers_ && destroy_swap_buffers) {
    // Tell any previous swapbuffers that it will no longer be used and can
    // destroy all its resources (and produce errors when used).
    swap_buffers_->Neuter();
    swap_buffers_ = nullptr;
  }
}

void GPUCanvasContext::FinalizeFrame(FlushReason) {
  // In some cases, such as when a canvas is hidden or offscreen, compositing
  // will never happen and thus OnTextureTransferred will never be called. In
  // those cases, getCurrentTexture is still required to return a new texture
  // after the current frame has ended, so we'll mark that a new texture is
  // required here.
  new_texture_required_ = true;
}

// WebGPUSwapBufferProvider::Client implementation
void GPUCanvasContext::OnTextureTransferred() {
  DCHECK(texture_);
  DCHECK(swap_texture_);

  if (copy_to_swap_texture_required_ && texture_ && !texture_->Destroyed()) {
    CopyToSwapTexture();
    texture_->ClearBeforeDestroyCallback();
    texture_->destroy();
  }
  texture_ = nullptr;
  swap_texture_ = nullptr;
}

void GPUCanvasContext::CopyToSwapTexture() {
  DCHECK(copy_to_swap_texture_required_);
  DCHECK(texture_);
  DCHECK(swap_texture_);

  if (!suppress_preferred_format_warning_) {
    // Will only display once per device
    device_->AddSingletonWarning(GPUSingletonWarning::kNonPreferredFormat);
  }

  WGPUImageCopyTexture source = {
      .texture = texture_->GetHandle(),
      .aspect = WGPUTextureAspect_All,
  };
  WGPUImageCopyTexture destination = {
      .texture = swap_texture_->GetHandle(),
      .aspect = WGPUTextureAspect_All,
  };

  gfx::Size size = swap_buffers_->Size();
  WGPUExtent3D copy_size = {
      .width = static_cast<uint32_t>(size.width()),
      .height = static_cast<uint32_t>(size.height()),
      .depthOrArrayLayers = 1,
  };

  WGPUAlphaMode copy_alpha_mode =
      alpha_mode_ == V8GPUCanvasAlphaMode::Enum::kOpaque
          ? WGPUAlphaMode_Opaque
          : WGPUAlphaMode_Premultiplied;

  WGPUCopyTextureForBrowserOptions options = {
      .srcAlphaMode = copy_alpha_mode,
      .dstAlphaMode = copy_alpha_mode,
      .internalUsage = true,
  };

  GetProcs().queueCopyTextureForBrowser(device_->queue()->GetHandle(), &source,
                                        &destination, &copy_size, &options);
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

  bool isOpaque = alpha_mode_ == V8GPUCanvasAlphaMode::Enum::kOpaque;

  // If either the texture is opaque or the texture format does not match the
  // resource provider's format then CopyTextureForBrowser will be used, which
  // performs a blit and can fix up the texture data during the copy.
  if (isOpaque || copy_to_swap_texture_required_) {
    WGPUAlphaMode srcAlphaMode =
        isOpaque ? WGPUAlphaMode_Opaque : WGPUAlphaMode_Premultiplied;

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
        dstAlphaMode = srcAlphaMode;
        break;
    }
    WGPUCopyTextureForBrowserOptions options = {
        .flipY = !resource_provider->IsOriginTopLeft(),
        .srcAlphaMode = srcAlphaMode,
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
  const auto info =
      SkImageInfo::Make(gfx::SizeToSkISize(size), canvas_context_color);
  // We tag the SharedImage inside the WebGPUImageProvider with display usage
  // since there are uncommon paths which may use this snapshot for compositing.
  // These paths are usually related to either printing or either video and
  // usually related to OffscreenCanvas; in cases where the image created from
  // this Snapshot will be sent eventually to the Display Compositor.
  auto resource_provider = CanvasResourceProvider::CreateWebGPUImageProvider(
      info, gpu::SHARED_IMAGE_USAGE_DISPLAY_READ);
  if (!resource_provider)
    return nullptr;

  if (!CopyTextureToResourceProvider(texture, size, resource_provider.get()))
    return nullptr;

  return resource_provider->Snapshot(FlushReason::kNone);
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
