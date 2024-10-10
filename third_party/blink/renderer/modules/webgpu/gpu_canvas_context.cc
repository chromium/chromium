// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"

#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlcanvaselement_offscreencanvas.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_alpha_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_tone_mapping.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_tone_mapping_mode.h"
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
  // it, and use it to create the resource provider.
  auto mailbox_texture = swap_buffers_->GetLastWebGPUMailboxTexture();
  if (!mailbox_texture) {
    return nullptr;
  }

  return SnapshotInternal(mailbox_texture->GetTexture(),
                          gfx::Size(mailbox_texture->GetTexture().GetWidth(),
                                    mailbox_texture->GetTexture().GetHeight()));
}

bool GPUCanvasContext::PaintRenderingResultsToCanvas(
    SourceDrawingBuffer source_buffer) {
  if (!swap_buffers_) {
    return false;
  }

  if (Host()->ResourceProvider() &&
      Host()->ResourceProvider()->Size() != swap_buffers_->Size()) {
    Host()->DiscardResourceProvider();
  }

  CanvasResourceProvider* resource_provider =
      Host()->GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
  if (!resource_provider) {
    return false;
  }

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

  // NOTE: It is necessary to call this before calling
  // PrepareTransferableResource(), as the latter moves the current swapbuffer
  // into `release_callback`.
  // TODO(crbug.com/353744937): Eliminate this code needing to prepare the
  // TransferableResource altogether in favor of it happening further down the
  // line via the ClientSI.
  auto client_si = swap_buffers_->GetCurrentSharedImage();
  viz::TransferableResource transferable_resource;
  viz::ReleaseCallback release_callback;
  if (!swap_buffers_->PrepareTransferableResource(
          nullptr, &transferable_resource, &release_callback)) {
    return false;
  }

  // If it was possible to prepare the transferable resource, the
  // ClientSharedImage must also be valid.
  CHECK(client_si);
  auto canvas_resource = ExternalCanvasResource::Create(
      std::move(client_si), transferable_resource, std::move(release_callback),
      GetContextProviderWeakPtr(), /*resource_provider=*/nullptr,
      filter_quality_,
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

  // NOTE: It is necessary to call this before calling
  // PrepareTransferableResource(), as the latter moves the current swapbuffer
  // into `release_callback`.
  // TODO(crbug.com/353744937): Eliminate this code needing to prepare the
  // TransferableResource altogether in favor of it happening further down the
  // line via the ClientSI.
  auto client_si = swap_buffers_->GetCurrentSharedImage();
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
  CHECK(client_si);

  // Use the sync token generated after producing the mailbox. Waiting for this
  // before trying to use the mailbox with some other context will ensure it is
  // valid.
  const auto& sk_image_sync_token = transferable_resource.sync_token();

  auto sk_color_type = viz::ToClosestSkColorType(
      /*gpu_compositing=*/true, transferable_resource.format);

  const SkImageInfo sk_image_info = SkImageInfo::Make(
      texture_descriptor_.size.width, texture_descriptor_.size.height,
      sk_color_type, kPremul_SkAlphaType);

  return MakeGarbageCollected<ImageBitmap>(
      AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
          std::move(client_si), sk_image_sync_token,
          /* shared_image_texture_id = */ 0, sk_image_info,
          transferable_resource.texture_target(),
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

  view_formats_ = AsDawnEnum<wgpu::TextureFormat>(descriptor->viewFormats());
  gfx::Size host_size = Host()->Size();

  // Set the default values of the member corresponding to
  // GPUCanvasContext.[[texture_descriptor]] in the WebGPU spec.
  texture_descriptor_ = {
      // Set the values from the configuration descriptor
      .usage = AsDawnFlags<wgpu::TextureUsage>(descriptor->usage()),
      .size = {static_cast<uint32_t>(host_size.width()),
               static_cast<uint32_t>(host_size.height())},
      .format = AsDawnEnum(descriptor->format()),
      .viewFormatCount = descriptor->viewFormats().size(),
      .viewFormats = view_formats_.data(),
      // Set the size of the texture in case there was no Reshape() since the
      // creation of the context.
  };

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
  device_->GetHandle().ValidateTextureDescriptor(&texture_descriptor_);

  copy_to_swap_texture_required_ = false;
  switch (texture_descriptor_.format) {
    case wgpu::TextureFormat::BGRA8Unorm:
#if BUILDFLAG(IS_ANDROID)
      // BGRA8Unorm is not natively supported by Android's compositor.
      copy_to_swap_texture_required_ = true;
#endif
      break;

    case wgpu::TextureFormat::RGBA8Unorm:
#if BUILDFLAG(IS_MAC)
      // RGBA8Unorm is not natively supported by MacOS's compositor.
      copy_to_swap_texture_required_ = true;
#endif
      break;

    case wgpu::TextureFormat::RGBA16Float:
      break;

    default:
      device_->InjectError(
          wgpu::ErrorType::Validation,
          ((String)("Unsupported canvas context format \"" +
                    FromDawnEnum(texture_descriptor_.format).AsString() + "\""))
              .Utf8()
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
      GPU::preferred_canvas_format() == wgpu::TextureFormat::BGRA8Unorm &&
      texture_descriptor_.usage & wgpu::TextureUsage::StorageBinding &&
      !device_->adapter()->features()->has(
          V8GPUFeatureName::Enum::kBgra8UnormStorage)) {
    suppress_preferred_format_warning_ = true;
  }

  alpha_mode_ = descriptor->alphaMode().AsEnum();

  // There are two scenarios that require special configuration here:
  // * In the scenario that the system doesn't support the requested format but
  //   it's one that WebGPU is required to offer, a separate texture will be
  //   returned instead with the desired format and the texture will be copied
  //   to the swap buffer provider's texture with the system-supported format
  //   when we're ready to present.
  // * In the alternative scenario where the texture returned to the user will
  //   be the swap buffer texture, the texture will have various internal
  //   operations done to it depending on the alpha mode.

  // First configure `texture_descriptor_` as necessary in the case where the
  // swap buffer texture will be returned to the user. Note that it is necessary
  // to do this *before* copying `texture_descriptor_` to
  // `swap_texture_descriptor_`: Each can end up being used in operations on the
  // texture depending on whether the operation is on `texture_` or
  // `swap_texture_` (which will of course be the same texture in this case).
  if (!copy_to_swap_texture_required_) {
    // `texture_` will be used as the source of either CopyTextureForBrowser()
    // or CopyTextureToTexture() operations (the former if the alpha mode is
    // opaque, the latter if it is not). In either case, CopySrc is required.
    texture_internal_usage_ = {{
        .internalUsage = wgpu::TextureUsage::CopySrc,
    }};
    if (alpha_mode_ == V8GPUCanvasAlphaMode::Enum::kOpaque) {
      // `texture_` will be used as the source of CopyTextureForBrowser()
      // operations and will have alpha clearing done on it. The former requires
      // the TextureBinding usage (in addition to the already-present CopySrc),
      // while the latter requires RenderAttachment.
      texture_internal_usage_.internalUsage |=
          wgpu::TextureUsage::TextureBinding |
          wgpu::TextureUsage::RenderAttachment;
    }

    texture_descriptor_.nextInChain = &texture_internal_usage_;
  }

  // Copy `texture_descriptor_` to `swap_texture_descriptor_` before making any
  // distinct configuration on each of them that is necessary in the case where
  // they will be distinct textures.
  swap_texture_descriptor_ = texture_descriptor_;
  if (copy_to_swap_texture_required_) {
    // The texture returned to the user will require both the CopySrc and
    // TextureBinding usages in order to be used with CopyTextureForBrowser.
    texture_internal_usage_ = {{
        .internalUsage =
            wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::TextureBinding,
    }};

    texture_descriptor_.nextInChain = &texture_internal_usage_;

    // The swap buffer texture will require both CopyDst and RenderAttachment
    // in order to be used with CopyTextureForBrowser.
    swap_texture_descriptor_.usage =
        wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::RenderAttachment;

    // In cases where a copy is necessary the swap buffers will always use the
    // preferred canvas format.
    swap_texture_descriptor_.format = GPU::preferred_canvas_format();

    // The swap buffer texture doesn't need any view formats.
    swap_texture_descriptor_.viewFormats = nullptr;
    swap_texture_descriptor_.viewFormatCount = 0;
  }

  if (!ValidateAndConvertColorSpace(descriptor->colorSpace(), color_space_,
                                    exception_state)) {
    return;
  }

  gfx::HDRMetadata hdr_metadata;
  if (descriptor->hasToneMapping() && descriptor->toneMapping()->hasMode()) {
    tone_mapping_mode_ = descriptor->toneMapping()->mode().AsEnum();
    switch (tone_mapping_mode_) {
      case V8GPUCanvasToneMappingMode::Enum::kStandard:
        break;
      case V8GPUCanvasToneMappingMode::Enum::kExtended:
        hdr_metadata.extended_range.emplace(
            /*current_headroom=*/gfx::HdrMetadataExtendedRange::
                kDefaultHdrHeadroom,
            /*desired_headroom=*/gfx::HdrMetadataExtendedRange::
                kDefaultHdrHeadroom);
        break;
    }
  }

  const wgpu::DawnTextureInternalUsageDescriptor* internal_usage_desc = nullptr;
  if (const wgpu::ChainedStruct* next_in_chain =
          swap_texture_descriptor_.nextInChain) {
    // The internal usage descriptor is the only valid struct to chain.
    CHECK_EQ(next_in_chain->sType,
             wgpu::SType::DawnTextureInternalUsageDescriptor);
    internal_usage_desc =
        static_cast<const wgpu::DawnTextureInternalUsageDescriptor*>(
            next_in_chain);
  }
  auto internal_usage = internal_usage_desc ? internal_usage_desc->internalUsage
                                            : wgpu::TextureUsage::None;
  swap_buffers_ = base::AdoptRef(new WebGPUSwapBufferProvider(
      this, device_->GetDawnControlClient(), device_->GetHandle(),
      swap_texture_descriptor_.usage, internal_usage,
      swap_texture_descriptor_.format, color_space_, hdr_metadata));
  swap_buffers_->SetFilterQuality(filter_quality_);

  // Note: SetContentsOpaque is only an optimization hint. It doesn't
  // actually make the contents opaque.
  switch (alpha_mode_) {
    case V8GPUCanvasAlphaMode::Enum::kOpaque: {
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

GPUCanvasConfiguration* GPUCanvasContext::getConfiguration() {
  if (!configured_) {
    return nullptr;
  }

  GPUCanvasConfiguration* configuration = GPUCanvasConfiguration::Create();
  configuration->setDevice(device_);
  configuration->setFormat(FromDawnEnum(texture_descriptor_.format));
  configuration->setUsage(static_cast<uint32_t>(texture_descriptor_.usage));

  Vector<V8GPUTextureFormat> view_formats;
  for (size_t i = 0; i < texture_descriptor_.viewFormatCount; ++i) {
    view_formats.push_back(FromDawnEnum(view_formats_[i]));
  }
  configuration->setViewFormats(view_formats);

  configuration->setColorSpace(PredefinedColorSpaceToV8(color_space_));
  configuration->setAlphaMode(alpha_mode_);

  GPUCanvasToneMapping* tone_mapping = GPUCanvasToneMapping::Create();
  tone_mapping->setMode(tone_mapping_mode_);
  configuration->setToneMapping(tone_mapping);

  return configuration;
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
    device_->InjectError(wgpu::ErrorType::Validation,
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
      device_->InjectError(wgpu::ErrorType::Validation,
                           "Could not create a swapchain texture of size 0.");
    } else {
      device_->InjectError(wgpu::ErrorType::Validation,
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
      static_cast<wgpu::TextureUsage>(swap_texture_descriptor_.usage),
      std::move(mailbox_texture),
      String::FromUTF8(swap_texture_descriptor_.label));

  if (copy_to_swap_texture_required_) {
    texture_ = MakeGarbageCollected<GPUTexture>(
        device_, device_->GetHandle().CreateTexture(&texture_descriptor_),
        String::FromUTF8(texture_descriptor_.label));
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

void GPUCanvasContext::SetNeedsCompositingUpdate() {
  if (Host()) {
    Host()->SetNeedsCompositingUpdate();
  }
}

void GPUCanvasContext::CopyToSwapTexture() {
  DCHECK(copy_to_swap_texture_required_);
  DCHECK(texture_);
  DCHECK(swap_texture_);

  if (!suppress_preferred_format_warning_) {
    // Will only display once per device
    device_->AddSingletonWarning(GPUSingletonWarning::kNonPreferredFormat);
  }

  wgpu::ImageCopyTexture source = {
      .texture = texture_->GetHandle(),
      .aspect = wgpu::TextureAspect::All,
  };
  wgpu::ImageCopyTexture destination = {
      .texture = swap_texture_->GetHandle(),
      .aspect = wgpu::TextureAspect::All,
  };

  gfx::Size size = swap_buffers_->Size();
  wgpu::Extent3D copy_size = {
      .width = static_cast<uint32_t>(size.width()),
      .height = static_cast<uint32_t>(size.height()),
      .depthOrArrayLayers = 1,
  };

  wgpu::AlphaMode copy_alpha_mode =
      alpha_mode_ == V8GPUCanvasAlphaMode::Enum::kOpaque
          ? wgpu::AlphaMode::Opaque
          : wgpu::AlphaMode::Premultiplied;

  wgpu::CopyTextureForBrowserOptions options = {
      .srcAlphaMode = copy_alpha_mode,
      .dstAlphaMode = copy_alpha_mode,
      .internalUsage = true,
  };

  device_->queue()->GetHandle().CopyTextureForBrowser(&source, &destination,
                                                      &copy_size, &options);
}

bool GPUCanvasContext::CopyTextureToResourceProvider(
    const wgpu::Texture& texture,
    const gfx::Size& size,
    CanvasResourceProvider* resource_provider) const {
  DCHECK(resource_provider);
  DCHECK_EQ(resource_provider->Size(), size);

  // This method will copy the contents of `texture` to `resource_provider`'s
  // backing SharedImage via the WebGPU interface. Hence, WEBGPU_WRITE usage
  // must be included on that backing SharedImage.
  DCHECK(resource_provider->GetSharedImageUsageFlags().Has(
      gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE));
  DCHECK(resource_provider->IsOriginTopLeft());

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> shared_context_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  if (!shared_context_wrapper || !shared_context_wrapper->ContextProvider())
    return false;

  auto dst_client_si =
      resource_provider->GetBackingClientSharedImageForOverwrite();
  if (!dst_client_si) {
    return false;
  }

  auto* ri = shared_context_wrapper->ContextProvider()->RasterInterface();

  if (!GetContextProviderWeakPtr()) {
    return false;
  }
  // todo(crbug/1267244) Use WebGPUMailboxTexture here instead of doing things
  // manually.
  gpu::webgpu::WebGPUInterface* webgpu =
      GetContextProviderWeakPtr()->ContextProvider()->WebGPUInterface();
  gpu::webgpu::ReservedTexture reservation =
      webgpu->ReserveTexture(device_->GetHandle().Get());
  DCHECK(reservation.texture);
  wgpu::Texture reserved_texture = wgpu::Texture::Acquire(reservation.texture);

  gpu::SyncToken sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  webgpu->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  wgpu::TextureUsage usage =
      wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::RenderAttachment;
  webgpu->AssociateMailbox(reservation.deviceId, reservation.deviceGeneration,
                           reservation.id, reservation.generation,
                           static_cast<uint64_t>(usage),
                           dst_client_si->mailbox());
  wgpu::ImageCopyTexture source = {
      .texture = texture,
      .aspect = wgpu::TextureAspect::All,
  };
  wgpu::ImageCopyTexture destination = {
      .texture = reserved_texture,
      .aspect = wgpu::TextureAspect::All,
  };
  wgpu::Extent3D copy_size = {
      .width = static_cast<uint32_t>(size.width()),
      .height = static_cast<uint32_t>(size.height()),
      .depthOrArrayLayers = 1,
  };

  bool isOpaque = alpha_mode_ == V8GPUCanvasAlphaMode::Enum::kOpaque;

  // If either the texture is opaque or the texture format does not match the
  // resource provider's format then CopyTextureForBrowser will be used, which
  // performs a blit and can fix up the texture data during the copy.
  if (isOpaque || copy_to_swap_texture_required_) {
    wgpu::AlphaMode srcAlphaMode =
        isOpaque ? wgpu::AlphaMode::Opaque : wgpu::AlphaMode::Premultiplied;

    // Issue a copyTextureForBrowser call with internal usage turned on.
    // There is a special step for srcAlphaMode == wgpu::AlphaMode::Opaque that
    // clears alpha channel to one.
    SkImageInfo sk_dst_image_info = resource_provider->GetSkImageInfo();
    wgpu::AlphaMode dstAlphaMode;
    switch (sk_dst_image_info.alphaType()) {
      case SkAlphaType::kPremul_SkAlphaType:
        dstAlphaMode = wgpu::AlphaMode::Premultiplied;
        break;
      case SkAlphaType::kUnpremul_SkAlphaType:
        dstAlphaMode = wgpu::AlphaMode::Unpremultiplied;
        break;
      case SkAlphaType::kOpaque_SkAlphaType:
        dstAlphaMode = wgpu::AlphaMode::Opaque;
        break;
      default:
        // Unknown dst alpha type, default to equal to src alpha mode
        dstAlphaMode = srcAlphaMode;
        break;
    }
    wgpu::CopyTextureForBrowserOptions options = {
        .flipY = !resource_provider->IsOriginTopLeft(),
        .srcAlphaMode = srcAlphaMode,
        .dstAlphaMode = dstAlphaMode,
        .internalUsage = true,
    };

    device_->queue()->GetHandle().CopyTextureForBrowser(&source, &destination,
                                                        &copy_size, &options);

  } else {
    // Create a command encoder and call copyTextureToTexture for the image
    // copy.
    wgpu::DawnEncoderInternalUsageDescriptor internal_usage_desc = {{
        .useInternalUsages = true,
    }};

    wgpu::CommandEncoderDescriptor command_encoder_desc = {
        .nextInChain = &internal_usage_desc,
    };
    wgpu::CommandEncoder command_encoder =
        device_->GetHandle().CreateCommandEncoder(&command_encoder_desc);
    command_encoder.CopyTextureToTexture(&source, &destination, &copy_size);

    wgpu::CommandBuffer command_buffer = command_encoder.Finish();
    command_encoder = nullptr;

    device_->queue()->GetHandle().Submit(1u, &command_buffer);
    command_buffer = nullptr;
  }

  webgpu->DissociateMailbox(reservation.id, reservation.generation);
  webgpu->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());

  return true;
}

scoped_refptr<StaticBitmapImage> GPUCanvasContext::SnapshotInternal(
    const wgpu::Texture& texture,
    const gfx::Size& size) const {
  const auto canvas_context_color = CanvasRenderingContextSkColorInfo();
  const auto info =
      SkImageInfo::Make(gfx::SizeToSkISize(size), canvas_context_color);
  // We tag the SharedImage inside the WebGPUImageProvider with display usages
  // since there are uncommon paths which may use this snapshot for compositing.
  // These paths are usually related to either printing or either video and
  // usually related to OffscreenCanvas; in cases where the image created from
  // this Snapshot will be sent eventually to the Display Compositor.
  auto resource_provider = CanvasResourceProvider::CreateWebGPUImageProvider(
      info, swap_buffers_->GetSharedImageUsagesForDisplay());
  if (!resource_provider)
    return nullptr;

  if (!CopyTextureToResourceProvider(texture, size, resource_provider.get()))
    return nullptr;

  return resource_provider->Snapshot(FlushReason::kNone);
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
GPUCanvasContext::GetContextProviderWeakPtr() const {
  return device_->GetDawnControlClient()->GetContextProviderWeakPtr();
}

}  // namespace blink
