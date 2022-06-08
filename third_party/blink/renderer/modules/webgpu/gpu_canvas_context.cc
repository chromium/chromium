// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlcanvaselement_offscreencanvas.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_alpha_mode.h"
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

void GPUCanvasContext::Reshape(int width, int height) {
  if (stopped_ || !swapchain_) {
    return;
  }

  // If an explicit size was given during the last call to configure() use that
  // size instead. This is deprecated behavior.
  // TODO(crbug.com/1326473): Remove after deprecation period.
  if (!configured_size_.IsZero()) {
    return;
  }

  ReconfigureSwapchain(gfx::Size(width, height));
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

  // This needs to happen early so that if any validation fails the swapchain
  // stays unconfigured.
  if (swapchain_) {
    // Tell any previous swapchain that it will no longer be used and can
    // destroy all its resources (and produce errors when used).
    swapchain_->Neuter();
    swapchain_ = nullptr;
  }

  // Store the configured device separately, even if the configuration fails, so
  // that errors can be generated in the appropriate error scope.
  configured_device_ = descriptor->device();

  usage_ = AsDawnFlags<WGPUTextureUsage>(descriptor->usage());
  format_ = AsDawnEnum(descriptor->format());
  switch (format_) {
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

  alpha_mode_ = V8GPUCanvasAlphaMode::Enum::kPremultiplied;
  if (descriptor->hasCompositingAlphaMode()) {
    alpha_mode_ = descriptor->compositingAlphaMode().AsEnum();
    configured_device_->AddConsoleWarning(
        "compositingAlphaMode is deprecated and will soon be removed. Please "
        "set alphaMode instead.");
  } else if (descriptor->hasAlphaMode()) {
    alpha_mode_ = descriptor->alphaMode().AsEnum();
  } else {
    configured_device_->AddConsoleWarning(
        "The default GPUCanvasAlphaMode will change from "
        "\"premultiplied\" to \"opaque\". "
        "Please explicitly set alphaMode to \"premultiplied\" if you would "
        "like to continue using that compositing mode.");
  }

  // TODO(crbug.com/1326473): Implement support for context viewFormats.
  if (descriptor->viewFormats().size()) {
    configured_device_->InjectError(
        WGPUErrorType_Validation,
        "Specifying additional viewFormats for GPUCanvasContexts is not "
        "supported yet.");
    return;
  }

  // TODO(crbug.com/1241375): Support additional color spaces for external
  // textures.
  if (descriptor->colorSpace().AsEnum() !=
      V8PredefinedColorSpace::Enum::kSRGB) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "colorSpace !== 'srgb' isn't supported yet.");
    return;
  }

  // Set the size while configuring.
  if (descriptor->hasSize()) {
    // TODO(crbug.com/1326473): Remove this branch after deprecation period.
    configured_device_->AddConsoleWarning(
        "Setting an explicit size when calling configure() on a "
        "GPUCanvasContext has been deprecated, and will soon be removed. "
        "Please set the canvas width and height attributes instead. Note that "
        "after the initial call to configure() changes to the canvas width and "
        "height will now take effect without the need to call configure() "
        "again.");

    WGPUExtent3D dawn_extent = AsDawnType(descriptor->size());
    configured_size_ = gfx::Size(dawn_extent.width, dawn_extent.height);

    if (dawn_extent.depthOrArrayLayers != 1) {
      configured_device_->InjectError(
          WGPUErrorType_Validation,
          "swap chain size must have depthOrArrayLayers set to 1");
      return;
    }
    if (configured_size_.IsEmpty()) {
      configured_device_->InjectError(
          WGPUErrorType_Validation,
          "context width and height must be greater than 0");
      return;
    }

    ReconfigureSwapchain(configured_size_);
  } else {
    configured_size_.SetSize(0, 0);
    ReconfigureSwapchain(Host()->Size());
  }
}

void GPUCanvasContext::ReconfigureSwapchain(gfx::Size size) {
  if (swapchain_) {
    // Tell any previous swapchain that it will no longer be used and can
    // destroy all its resources (and produce errors when used).
    swapchain_->Neuter();
    swapchain_ = nullptr;
  }

  swapchain_ = MakeGarbageCollected<GPUSwapChain>(
      this, configured_device_, usage_, format_, filter_quality_, alpha_mode_,
      size);

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

String GPUCanvasContext::getPreferredFormat(ExecutionContext* execution_context,
                                            GPUAdapter* adapter) {
  adapter->AddConsoleWarning(
      execution_context,
      "Calling getPreferredFormat() on a GPUCanvasContext is deprecated and "
      "will soon be removed. Call navigator.gpu.getPreferredCanvasFormat() "
      "instead, which no longer requires an adapter.");
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
