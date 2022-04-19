// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlcanvaselement_offscreencanvas.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_compositing_alpha_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasrenderingcontext2d_gpucanvascontext_imagebitmaprenderingcontext_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpucanvascontext_imagebitmaprenderingcontext_offscreencanvasrenderingcontext2d_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

namespace blink {

GPUCanvasContext::Factory::Factory() {}
GPUCanvasContext::Factory::~Factory() {}

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
    : CanvasRenderingContext(host, attrs, CanvasRenderingAPI::kWebgpu) {}

GPUCanvasContext::~GPUCanvasContext() {}

void GPUCanvasContext::Trace(Visitor* visitor) const {
  visitor->Trace(swapchain_);
  visitor->Trace(configured_device_);
  CanvasRenderingContext::Trace(visitor);
}

// CanvasRenderingContext implementation
V8RenderingContext* GPUCanvasContext::AsV8RenderingContext() {
  return MakeGarbageCollected<V8RenderingContext>(this);
}

V8OffscreenRenderingContext* GPUCanvasContext::AsV8OffscreenRenderingContext() {
  return MakeGarbageCollected<V8OffscreenRenderingContext>(this);
}

void GPUCanvasContext::Stop() {
  if (swapchain_) {
    swapchain_->Neuter();
    swapchain_ = nullptr;
  }
  stopped_ = true;
}

cc::Layer* GPUCanvasContext::CcLayer() const {
  if (swapchain_) {
    return swapchain_->CcLayer();
  }
  return nullptr;
}

scoped_refptr<StaticBitmapImage> GPUCanvasContext::GetImage() {
  if (!swapchain_)
    return nullptr;

  return swapchain_->Snapshot();
}

bool GPUCanvasContext::PaintRenderingResultsToCanvas(
    SourceDrawingBuffer source_buffer) {
  DCHECK_EQ(source_buffer, kBackBuffer);
  if (!swapchain_)
    return false;

  if (Host()->ResourceProvider() &&
      Host()->ResourceProvider()->Size() != swapchain_->Size()) {
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
  DCHECK_EQ(source_buffer, kBackBuffer);
  if (swapchain_)
    return swapchain_->CopyToResourceProvider(resource_provider);
  return false;
}

void GPUCanvasContext::SetFilterQuality(
    cc::PaintFlags::FilterQuality filter_quality) {
  if (filter_quality != filter_quality_) {
    filter_quality_ = filter_quality;
    if (swapchain_) {
      swapchain_->SetFilterQuality(filter_quality);
    }
  }
}

bool GPUCanvasContext::PushFrame() {
  DCHECK(Host());
  DCHECK(Host()->IsOffscreenCanvas());
  auto canvas_resource = swapchain_->ExportCanvasResource();
  if (!canvas_resource)
    return false;
  const int width = canvas_resource->Size().width();
  const int height = canvas_resource->Size().height();
  return Host()->PushFrame(std::move(canvas_resource),
                           SkIRect::MakeWH(width, height));
}

ImageBitmap* GPUCanvasContext::TransferToImageBitmap(
    ScriptState* script_state) {
  return MakeGarbageCollected<ImageBitmap>(
      swapchain_->TransferToStaticBitmapImage());
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

  if (swapchain_) {
    // Tell any previous swapchain that it will no longer be used and can
    // destroy all its resources (and produce errors when used).
    swapchain_->Neuter();
    swapchain_ = nullptr;
  }

  // Store the configured device separately, even if the configuration fails, so
  // that errors can be generated in the appropriate error scope.
  configured_device_ = descriptor->device();

  WGPUTextureUsage usage = AsDawnEnum<WGPUTextureUsage>(descriptor->usage());
  WGPUTextureFormat format =
      AsDawnEnum<WGPUTextureFormat>(descriptor->format());
  switch (format) {
    case WGPUTextureFormat_BGRA8Unorm:
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
      configured_device_->InjectError(WGPUErrorType_Validation,
                                      "unsupported swap chain format");
      return;
  }

  // Set the default size.
  gfx::Size size;
  if (descriptor->hasSize()) {
    WGPUExtent3D dawn_extent = AsDawnType(descriptor->size());
    size = gfx::Size(dawn_extent.width, dawn_extent.height);

    if (dawn_extent.depthOrArrayLayers != 1) {
      configured_device_->InjectError(
          WGPUErrorType_Validation,
          "swap chain size must have depthOrArrayLayers set to 1");
      return;
    }
    if (size.IsEmpty()) {
      configured_device_->InjectError(
          WGPUErrorType_Validation,
          "context width and height must be greater than 0");
      return;
    }
  } else {
    size = Host()->Size();
  }

  V8GPUCanvasCompositingAlphaMode::Enum alpha_mode =
      V8GPUCanvasCompositingAlphaMode::Enum::kPremultiplied;
  if (descriptor->hasCompositingAlphaMode()) {
    alpha_mode = descriptor->compositingAlphaMode().AsEnum();
  } else {
    configured_device_->AddConsoleWarning(
        "The default GPUCanvasCompositingAlphaMode will change from \"premultiplied\" to \"opaque\". "
        "Please explicitly pass \"premultiplied\" if you would like to "
        "continue using that compositing mode.");
  }
  swapchain_ = MakeGarbageCollected<GPUSwapChain>(
      this, configured_device_, usage, format, filter_quality_, alpha_mode,
      size);

  if (descriptor->hasLabel())
    swapchain_->setLabel(descriptor->label());

  // If we don't notify the host that something has changed it may never check
  // for the new cc::Layer.
  Host()->SetNeedsCompositingUpdate();
}

void GPUCanvasContext::unconfigure() {
  if (stopped_) {
    return;
  }

  if (swapchain_) {
    // Tell any previous swapchain that it will no longer be used and can
    // destroy all its resources (and produce errors when used).
    swapchain_->Neuter();
    swapchain_ = nullptr;
  }

  configured_device_ = nullptr;
}

String GPUCanvasContext::getPreferredFormat(const GPUAdapter* adapter) {
  // TODO(crbug.com/1007166): Return actual preferred format for the swap chain.
  return "bgra8unorm";
}

GPUTexture* GPUCanvasContext::getCurrentTexture(
    ExceptionState& exception_state) {
  if (!configured_device_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "context is not configured");
    return nullptr;
  }
  if (!swapchain_) {
    configured_device_->InjectError(WGPUErrorType_Validation,
                                    "context configuration is invalid.");
    return GPUTexture::CreateError(configured_device_);
  }
  return swapchain_->getCurrentTexture();
}

}  // namespace blink
