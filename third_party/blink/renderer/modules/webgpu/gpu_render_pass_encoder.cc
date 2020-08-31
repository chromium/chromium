// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_pass_encoder.h"

#include "third_party/blink/renderer/bindings/modules/v8/double_sequence_or_gpu_color_dict.h"
#include "third_party/blink/renderer/core/typed_arrays/typed_flexible_array_buffer_view.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"

namespace blink {

GPURenderPassEncoder::GPURenderPassEncoder(
    GPUDevice* device,
    WGPURenderPassEncoder render_pass_encoder)
    : DawnObject<WGPURenderPassEncoder>(device, render_pass_encoder) {}

GPURenderPassEncoder::~GPURenderPassEncoder() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().renderPassEncoderRelease(GetHandle());
}

void GPURenderPassEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bindGroup,
    const Vector<uint32_t>& dynamicOffsets) {
  GetProcs().renderPassEncoderSetBindGroup(
      GetHandle(), index, bindGroup->GetHandle(), dynamicOffsets.size(),
      dynamicOffsets.data());
}

void GPURenderPassEncoder::setBindGroup(
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

  GetProcs().renderPassEncoderSetBindGroup(GetHandle(), index,
                                           bind_group->GetHandle(),
                                           dynamic_offsets_data_length, data);
}

void GPURenderPassEncoder::pushDebugGroup(String groupLabel) {
  std::string label = groupLabel.Utf8();
  GetProcs().renderPassEncoderPushDebugGroup(GetHandle(), label.c_str());
}

void GPURenderPassEncoder::popDebugGroup() {
  GetProcs().renderPassEncoderPopDebugGroup(GetHandle());
}

void GPURenderPassEncoder::insertDebugMarker(String markerLabel) {
  std::string label = markerLabel.Utf8();
  GetProcs().renderPassEncoderInsertDebugMarker(GetHandle(), label.c_str());
}

void GPURenderPassEncoder::setPipeline(GPURenderPipeline* pipeline) {
  GetProcs().renderPassEncoderSetPipeline(GetHandle(), pipeline->GetHandle());
}

void GPURenderPassEncoder::setBlendColor(DoubleSequenceOrGPUColorDict& color,
                                         ExceptionState& exception_state) {
  if (color.IsDoubleSequence() && color.GetAsDoubleSequence().size() != 4) {
    exception_state.ThrowRangeError("color size must be 4");
    return;
  }

  WGPUColor dawn_color = AsDawnType(&color);
  GetProcs().renderPassEncoderSetBlendColor(GetHandle(), &dawn_color);
}

void GPURenderPassEncoder::setStencilReference(uint32_t reference) {
  GetProcs().renderPassEncoderSetStencilReference(GetHandle(), reference);
}

void GPURenderPassEncoder::setViewport(float x,
                                       float y,
                                       float width,
                                       float height,
                                       float minDepth,
                                       float maxDepth) {
  GetProcs().renderPassEncoderSetViewport(GetHandle(), x, y, width, height,
                                          minDepth, maxDepth);
}

void GPURenderPassEncoder::setScissorRect(uint32_t x,
                                          uint32_t y,
                                          uint32_t width,
                                          uint32_t height) {
  GetProcs().renderPassEncoderSetScissorRect(GetHandle(), x, y, width, height);
}

void GPURenderPassEncoder::setIndexBuffer(GPUBuffer* buffer,
                                          uint64_t offset,
                                          uint64_t size) {
  device_->AddConsoleWarning(
      "Calling setIndexBuffer without a GPUIndexFormat is deprecated.");
  GetProcs().renderPassEncoderSetIndexBuffer(GetHandle(), buffer->GetHandle(),
                                             offset, size);
}

void GPURenderPassEncoder::setIndexBuffer(GPUBuffer* buffer,
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
  GetProcs().renderPassEncoderSetIndexBufferWithFormat(
      GetHandle(), buffer->GetHandle(), AsDawnEnum<WGPUIndexFormat>(format),
      offset, size);
}

void GPURenderPassEncoder::setVertexBuffer(uint32_t slot,
                                           const GPUBuffer* buffer,
                                           const uint64_t offset,
                                           const uint64_t size) {
  GetProcs().renderPassEncoderSetVertexBuffer(
      GetHandle(), slot, buffer->GetHandle(), offset, size);
}

void GPURenderPassEncoder::draw(uint32_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t firstInstance) {
  GetProcs().renderPassEncoderDraw(GetHandle(), vertexCount, instanceCount,
                                   firstVertex, firstInstance);
}

void GPURenderPassEncoder::drawIndexed(uint32_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t baseVertex,
                                       uint32_t firstInstance) {
  GetProcs().renderPassEncoderDrawIndexed(GetHandle(), indexCount,
                                          instanceCount, firstIndex, baseVertex,
                                          firstInstance);
}

void GPURenderPassEncoder::drawIndirect(GPUBuffer* indirectBuffer,
                                        uint64_t indirectOffset) {
  GetProcs().renderPassEncoderDrawIndirect(
      GetHandle(), indirectBuffer->GetHandle(), indirectOffset);
}

void GPURenderPassEncoder::drawIndexedIndirect(GPUBuffer* indirectBuffer,
                                               uint64_t indirectOffset) {
  GetProcs().renderPassEncoderDrawIndexedIndirect(
      GetHandle(), indirectBuffer->GetHandle(), indirectOffset);
}

void GPURenderPassEncoder::executeBundles(
    const HeapVector<Member<GPURenderBundle>>& bundles) {
  std::unique_ptr<WGPURenderBundle[]> dawn_bundles = AsDawnType(bundles);

  GetProcs().renderPassEncoderExecuteBundles(GetHandle(), bundles.size(),
                                             dawn_bundles.get());
}

void GPURenderPassEncoder::endPass() {
  GetProcs().renderPassEncoderEndPass(GetHandle());
}

}  // namespace blink
