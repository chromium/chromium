// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"

#include "third_party/blink/renderer/bindings/modules/v8/offscreen_rendering_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/rendering_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_swap_chain_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasrenderingcontext2d_gpucanvascontext_imagebitmaprenderingcontext_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpucanvascontext_imagebitmaprenderingcontext_offscreencanvasrenderingcontext2d_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

GPUCanvasContext::Factory::Factory() {}
GPUCanvasContext::Factory::~Factory() {}

CanvasRenderingContext* GPUCanvasContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  CanvasRenderingContext* rendering_context =
      MakeGarbageCollected<GPUCanvasContext>(host, attrs);
  DCHECK(host);
  rendering_context->RecordUKMCanvasRenderingAPI(
      CanvasRenderingContext::CanvasRenderingAPI::kWebgpu);
  return rendering_context;
}

CanvasRenderingContext::ContextType GPUCanvasContext::Factory::GetContextType()
    const {
  return CanvasRenderingContext::kContextGPUPresent;
}

GPUCanvasContext::GPUCanvasContext(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs)
    : CanvasRenderingContext(host, attrs) {}

GPUCanvasContext::~GPUCanvasContext() {}

void GPUCanvasContext::Trace(Visitor* visitor) const {
  visitor->Trace(swapchain_);
  CanvasRenderingContext::Trace(visitor);
}

const IntSize& GPUCanvasContext::CanvasSize() const {
  return Host()->Size();
}

// CanvasRenderingContext implementation
CanvasRenderingContext::ContextType GPUCanvasContext::GetContextType() const {
  return CanvasRenderingContext::kContextGPUPresent;
}

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

V8RenderingContext* GPUCanvasContext::AsV8RenderingContext() {
  return MakeGarbageCollected<V8RenderingContext>(this);
}

V8OffscreenRenderingContext* GPUCanvasContext::AsV8OffscreenRenderingContext() {
  return MakeGarbageCollected<V8OffscreenRenderingContext>(this);
}

#else  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

void GPUCanvasContext::SetCanvasGetContextResult(RenderingContext& result) {
  result.SetGPUCanvasContext(this);
}

void GPUCanvasContext::SetOffscreenCanvasGetContextResult(
    OffscreenRenderingContext& result) {
  result.SetGPUCanvasContext(this);
}

#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

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

void GPUCanvasContext::SetFilterQuality(SkFilterQuality filter_quality) {
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
  const int width = canvas_resource->Size().Width();
  const int height = canvas_resource->Size().Height();
  return Host()->PushFrame(std::move(canvas_resource),
                           SkIRect::MakeWH(width, height));
}

ImageBitmap* GPUCanvasContext::TransferToImageBitmap(
    ScriptState* script_state) {
  return MakeGarbageCollected<ImageBitmap>(
      swapchain_->TransferToStaticBitmapImage());
}

// gpu_canvas_context.idl
GPUSwapChain* GPUCanvasContext::configureSwapChain(
    const GPUSwapChainDescriptor* descriptor,
    ExceptionState& exception_state) {
  if (stopped_) {
    // This is probably not possible, or at least would only happen during page
    // shutdown.
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "canvas has been destroyed");
    return nullptr;
  }

  if (swapchain_) {
    // Tell any previous swapchain that it will no longer be used and can
    // destroy all its resources (and produce errors when used).
    swapchain_->Neuter();
  }

  WGPUTextureUsage usage = AsDawnEnum<WGPUTextureUsage>(descriptor->usage());
  WGPUTextureFormat format =
      AsDawnEnum<WGPUTextureFormat>(descriptor->format());
  switch (format) {
    case WGPUTextureFormat_BGRA8Unorm:
      break;
    case WGPUTextureFormat_RGBA16Float:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kUnknownError,
          "rgba16float swap chain is not yet supported");
      return nullptr;
    default:
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        "unsupported swap chain format");
      return nullptr;
  }

  swapchain_ = MakeGarbageCollected<GPUSwapChain>(
      this, descriptor->device(), usage, format, filter_quality_);
  swapchain_->CcLayer()->SetContentsOpaque(!CreationAttributes().alpha);
  swapchain_->setLabel(descriptor->label());

  // If we don't notify the host that something has changed it may never check
  // for the new cc::Layer.
  Host()->SetNeedsCompositingUpdate();

  return swapchain_;
}

String GPUCanvasContext::getSwapChainPreferredFormat(
    const GPUAdapter* adapter) {
  // TODO(crbug.com/1007166): Return actual preferred format for the swap chain.
  return "bgra8unorm";
}

}  // namespace blink
