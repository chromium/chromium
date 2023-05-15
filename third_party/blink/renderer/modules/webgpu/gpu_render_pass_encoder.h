// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PASS_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PASS_ENCODER_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_programmable_pass_encoder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class GPUBindGroup;
class GPURenderBundle;
class V8GPUIndexFormat;

class GPURenderPassEncoder : public DawnObject<WGPURenderPassEncoder>,
                             public GPUProgrammablePassEncoder {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPURenderPassEncoder(GPUDevice* device,
                                WGPURenderPassEncoder render_pass_encoder);

  GPURenderPassEncoder(const GPURenderPassEncoder&) = delete;
  GPURenderPassEncoder& operator=(const GPURenderPassEncoder&) = delete;

  // gpu_render_pass_encoder.idl
  void setBindGroup(uint32_t index, DawnObject<WGPUBindGroup>* bindGroup) {
    GetProcs().renderPassEncoderSetBindGroup(
        GetHandle(), index, bindGroup->GetHandle(), 0, nullptr);
  }
  void setBindGroup(uint32_t index,
                    GPUBindGroup* bindGroup,
                    const Vector<uint32_t>& dynamicOffsets);
  void setBindGroup(uint32_t index,
                    GPUBindGroup* bind_group,
                    const FlexibleUint32Array& dynamic_offsets_data,
                    uint64_t dynamic_offsets_data_start,
                    uint32_t dynamic_offsets_data_length,
                    ExceptionState& exception_state);
  void pushDebugGroup(String groupLabel) {
    std::string label = groupLabel.Utf8();
    GetProcs().renderPassEncoderPushDebugGroup(GetHandle(), label.c_str());
  }
  void popDebugGroup() {
    GetProcs().renderPassEncoderPopDebugGroup(GetHandle());
  }
  void insertDebugMarker(String markerLabel) {
    std::string label = markerLabel.Utf8();
    GetProcs().renderPassEncoderInsertDebugMarker(GetHandle(), label.c_str());
  }
  void setPipeline(const DawnObject<WGPURenderPipeline>* pipeline) {
    GetProcs().renderPassEncoderSetPipeline(GetHandle(), pipeline->GetHandle());
  }

  void setBlendConstant(const V8GPUColor* color,
                        ExceptionState& exception_state);
  void setStencilReference(uint32_t reference) {
    GetProcs().renderPassEncoderSetStencilReference(GetHandle(), reference);
  }
  void setViewport(float x,
                   float y,
                   float width,
                   float height,
                   float minDepth,
                   float maxDepth) {
    GetProcs().renderPassEncoderSetViewport(GetHandle(), x, y, width, height,
                                            minDepth, maxDepth);
  }
  void setScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    GetProcs().renderPassEncoderSetScissorRect(GetHandle(), x, y, width,
                                               height);
  }
  void setIndexBuffer(const DawnObject<WGPUBuffer>* buffer,
                      const V8GPUIndexFormat& format,
                      uint64_t offset) {
    GetProcs().renderPassEncoderSetIndexBuffer(GetHandle(), buffer->GetHandle(),
                                               AsDawnEnum(format), offset,
                                               WGPU_WHOLE_SIZE);
  }
  void setIndexBuffer(const DawnObject<WGPUBuffer>* buffer,
                      const V8GPUIndexFormat& format,
                      uint64_t offset,
                      uint64_t size) {
    GetProcs().renderPassEncoderSetIndexBuffer(
        GetHandle(), buffer->GetHandle(), AsDawnEnum(format), offset, size);
  }
  void setVertexBuffer(uint32_t slot,
                       const DawnObject<WGPUBuffer>* buffer,
                       uint64_t offset) {
    GetProcs().renderPassEncoderSetVertexBuffer(
        GetHandle(), slot, buffer->GetHandle(), offset, WGPU_WHOLE_SIZE);
  }
  void setVertexBuffer(uint32_t slot,
                       const DawnObject<WGPUBuffer>* buffer,
                       uint64_t offset,
                       uint64_t size) {
    GetProcs().renderPassEncoderSetVertexBuffer(
        GetHandle(), slot, buffer->GetHandle(), offset, size);
  }
  void draw(uint32_t vertexCount,
            uint32_t instanceCount,
            uint32_t firstVertex,
            uint32_t firstInstance) {
    GetProcs().renderPassEncoderDraw(GetHandle(), vertexCount, instanceCount,
                                     firstVertex, firstInstance);
  }
  void drawIndexed(uint32_t indexCount,
                   uint32_t instanceCount,
                   uint32_t firstIndex,
                   int32_t baseVertex,
                   uint32_t firstInstance) {
    GetProcs().renderPassEncoderDrawIndexed(GetHandle(), indexCount,
                                            instanceCount, firstIndex,
                                            baseVertex, firstInstance);
  }
  void drawIndirect(const DawnObject<WGPUBuffer>* indirectBuffer,
                    uint64_t indirectOffset) {
    GetProcs().renderPassEncoderDrawIndirect(
        GetHandle(), indirectBuffer->GetHandle(), indirectOffset);
  }
  void drawIndexedIndirect(const DawnObject<WGPUBuffer>* indirectBuffer,
                           uint64_t indirectOffset) {
    GetProcs().renderPassEncoderDrawIndexedIndirect(
        GetHandle(), indirectBuffer->GetHandle(), indirectOffset);
  }
  void executeBundles(const HeapVector<Member<GPURenderBundle>>& bundles);
  void beginOcclusionQuery(uint32_t queryIndex) {
    GetProcs().renderPassEncoderBeginOcclusionQuery(GetHandle(), queryIndex);
  }
  void endOcclusionQuery() {
    GetProcs().renderPassEncoderEndOcclusionQuery(GetHandle());
  }
  void writeTimestamp(const DawnObject<WGPUQuerySet>* querySet,
                      uint32_t queryIndex,
                      ExceptionState& exception_state);
  void end() { GetProcs().renderPassEncoderEnd(GetHandle()); }

  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().renderPassEncoderSetLabel(GetHandle(), utf8_label.c_str());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PASS_ENCODER_H_
