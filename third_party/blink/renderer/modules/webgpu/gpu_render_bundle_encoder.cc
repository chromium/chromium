// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle_encoder.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_bundle_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_bundle_encoder_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

// static
GPURenderBundleEncoder* GPURenderBundleEncoder::Create(
    GPUDevice* device,
    const GPURenderBundleEncoderDescriptor* webgpu_desc) {
  uint32_t color_formats_count =
      static_cast<uint32_t>(webgpu_desc->colorFormats().size());

  std::unique_ptr<WGPUTextureFormat[]> color_formats =
      AsDawnEnum<WGPUTextureFormat>(webgpu_desc->colorFormats());

  WGPUTextureFormat depth_stencil_format = WGPUTextureFormat_Undefined;
  if (webgpu_desc->hasDepthStencilFormat()) {
    depth_stencil_format =
        AsDawnEnum<WGPUTextureFormat>(webgpu_desc->depthStencilFormat());
  }

  std::string label;
  WGPURenderBundleEncoderDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.colorFormatsCount = color_formats_count;
  dawn_desc.colorFormats = color_formats.get();
  dawn_desc.depthStencilFormat = depth_stencil_format;
  dawn_desc.sampleCount = webgpu_desc->sampleCount();
  if (webgpu_desc->hasLabel()) {
    label = webgpu_desc->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  return MakeGarbageCollected<GPURenderBundleEncoder>(
      device, device->GetProcs().deviceCreateRenderBundleEncoder(
                  device->GetHandle(), &dawn_desc));
}

GPURenderBundleEncoder::GPURenderBundleEncoder(
    GPUDevice* device,
    WGPURenderBundleEncoder render_bundle_encoder)
    : DawnObject<WGPURenderBundleEncoder>(device, render_bundle_encoder) {}

GPURenderBundleEncoder::~GPURenderBundleEncoder() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().renderBundleEncoderRelease(GetHandle());
}

void GPURenderBundleEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bindGroup,
    const Vector<uint32_t>& dynamicOffsets) {
  GetProcs().renderBundleEncoderSetBindGroup(
      GetHandle(), index, bindGroup->GetHandle(), dynamicOffsets.size(),
      dynamicOffsets.data());
}

void GPURenderBundleEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bind_group,
    const FlexibleUint32Array& dynamic_offsets_data,
    uint64_t dynamic_offsets_data_start,
    uint32_t dynamic_offsets_data_length,
    ExceptionState& exception_state) {
  if (!ValidateSetBindGroupDynamicOffsets(
          dynamic_offsets_data, dynamic_offsets_data_start,
          dynamic_offsets_data_length, exception_state)) {
    return;
  }

  const uint32_t* data =
      dynamic_offsets_data.DataMaybeOnStack() + dynamic_offsets_data_start;

  GetProcs().renderBundleEncoderSetBindGroup(GetHandle(), index,
                                             bind_group->GetHandle(),
                                             dynamic_offsets_data_length, data);
}

void GPURenderBundleEncoder::pushDebugGroup(String groupLabel) {
  std::string label = groupLabel.Utf8();
  GetProcs().renderBundleEncoderPushDebugGroup(GetHandle(), label.c_str());
}

void GPURenderBundleEncoder::popDebugGroup() {
  GetProcs().renderBundleEncoderPopDebugGroup(GetHandle());
}

void GPURenderBundleEncoder::insertDebugMarker(String markerLabel) {
  std::string label = markerLabel.Utf8();
  GetProcs().renderBundleEncoderInsertDebugMarker(GetHandle(), label.c_str());
}

void GPURenderBundleEncoder::setPipeline(GPURenderPipeline* pipeline) {
  GetProcs().renderBundleEncoderSetPipeline(GetHandle(), pipeline->GetHandle());
}

void GPURenderBundleEncoder::setIndexBuffer(GPUBuffer* buffer,
                                            uint64_t offset,
                                            uint64_t size) {
  device_->AddConsoleWarning(
      "Calling setIndexBuffer without a GPUIndexFormat is deprecated.");
  GetProcs().renderBundleEncoderSetIndexBuffer(GetHandle(), buffer->GetHandle(),
                                               offset, size);
}

void GPURenderBundleEncoder::setIndexBuffer(GPUBuffer* buffer,
                                            const WTF::String& format,
                                            uint64_t offset,
                                            uint64_t size,
                                            ExceptionState& exception_state) {
  if (format != "uint16" && format != "uint32") {
    exception_state.ThrowTypeError(
        "The provided value '" + format +
        "' is not a valid enum value of type GPUIndexFormat.");
    return;
  }
  GetProcs().renderBundleEncoderSetIndexBufferWithFormat(
      GetHandle(), buffer->GetHandle(), AsDawnEnum<WGPUIndexFormat>(format),
      offset, size);
}

void GPURenderBundleEncoder::setVertexBuffer(uint32_t slot,
                                             const GPUBuffer* buffer,
                                             uint64_t offset,
                                             uint64_t size) {
  GetProcs().renderBundleEncoderSetVertexBuffer(
      GetHandle(), slot, buffer->GetHandle(), offset, size);
}

void GPURenderBundleEncoder::draw(uint32_t vertexCount,
                                  uint32_t instanceCount,
                                  uint32_t firstVertex,
                                  uint32_t firstInstance) {
  GetProcs().renderBundleEncoderDraw(GetHandle(), vertexCount, instanceCount,
                                     firstVertex, firstInstance);
}

void GPURenderBundleEncoder::draw(uint32_t vertexCount,
                                  uint32_t instanceCount,
                                  uint32_t firstVertex,
                                  uint32_t firstInstance,
                                  v8::FastApiCallbackOptions& options) {
  draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void GPURenderBundleEncoder::drawIndexed(uint32_t indexCount,
                                         uint32_t instanceCount,
                                         uint32_t firstIndex,
                                         int32_t baseVertex,
                                         uint32_t firstInstance) {
  GetProcs().renderBundleEncoderDrawIndexed(GetHandle(), indexCount,
                                            instanceCount, firstIndex,
                                            baseVertex, firstInstance);
}

void GPURenderBundleEncoder::drawIndexed(uint32_t indexCount,
                                         uint32_t instanceCount,
                                         uint32_t firstIndex,
                                         int32_t baseVertex,
                                         uint32_t firstInstance,
                                         v8::FastApiCallbackOptions& options) {
  drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void GPURenderBundleEncoder::drawIndirect(GPUBuffer* indirectBuffer,
                                          uint64_t indirectOffset) {
  GetProcs().renderBundleEncoderDrawIndirect(
      GetHandle(), indirectBuffer->GetHandle(), indirectOffset);
}

void GPURenderBundleEncoder::drawIndexedIndirect(GPUBuffer* indirectBuffer,
                                                 uint64_t indirectOffset) {
  GetProcs().renderBundleEncoderDrawIndexedIndirect(
      GetHandle(), indirectBuffer->GetHandle(), indirectOffset);
}

GPURenderBundle* GPURenderBundleEncoder::finish(
    const GPURenderBundleDescriptor* webgpu_desc) {
  std::string label;
  WGPURenderBundleDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  if (webgpu_desc->hasLabel()) {
    label = webgpu_desc->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  WGPURenderBundle render_bundle =
      GetProcs().renderBundleEncoderFinish(GetHandle(), &dawn_desc);
  return MakeGarbageCollected<GPURenderBundle>(device_, render_bundle);
}

}  // namespace blink
