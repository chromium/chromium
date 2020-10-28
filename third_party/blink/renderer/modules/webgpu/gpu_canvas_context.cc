// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"

#include "third_party/blink/renderer/bindings/modules/v8/rendering_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_swap_chain_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
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

void GPUCanvasContext::SetCanvasGetContextResult(RenderingContext& result) {
  result.SetGPUCanvasContext(this);
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

void GPUCanvasContext::SetFilterQuality(SkFilterQuality filter_quality) {
  if (filter_quality != filter_quality_) {
    filter_quality_ = filter_quality;
    if (swapchain_) {
      swapchain_->SetFilterQuality(filter_quality);
    }
  }
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
    case WGPUTextureFormat_RGBA8Unorm:
      if ((usage & WGPUTextureUsage_OutputAttachment) != usage) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kOperationError,
            "rgba8unorm can only support OUTPUT_ATTACHMENT usage");
      }
      descriptor->device()->AddConsoleWarning(
          "rgba8unorm swap chain is deprecated (for now); use bgra8unorm");
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
  return swapchain_;
}

ScriptPromise GPUCanvasContext::getSwapChainPreferredFormat(
    ScriptState* script_state,
    const GPUDevice* device) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // TODO(crbug.com/1007166): Return actual preferred format for the swap chain.
  resolver->Resolve("bgra8unorm");

  return promise;
}

}  // namespace blink
