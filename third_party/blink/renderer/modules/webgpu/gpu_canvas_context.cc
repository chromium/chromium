// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"

#include "third_party/blink/renderer/bindings/modules/v8/rendering_context.h"

namespace blink {

GPUCanvasContext::Factory::Factory() {}
GPUCanvasContext::Factory::~Factory() {}

CanvasRenderingContext* GPUCanvasContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  return MakeGarbageCollected<GPUCanvasContext>(host, attrs);
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

void GPUCanvasContext::Trace(blink::Visitor* visitor) {
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

// gpu_canvas_context.idl
GPUSwapChain* GPUCanvasContext::configureSwapChain(
    const GPUSwapChainDescriptor* descriptor) {
  // TODO(cwallez@chromium.org): This should probably throw an exception,
  // implement the exception when the WebGPU group decided what it should be.
  if (stopped_) {
    return nullptr;
  }

  if (swapchain_) {
    // Tell any previous swapchain that it will no longer be used and can
    // destroy all its resources (and produce errors when used).
    swapchain_->Neuter();
  }
  swapchain_ = GPUSwapChain::Create(this, descriptor);
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
