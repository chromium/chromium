// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PASS_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PASS_ENCODER_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_programmable_pass_encoder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class GPUBindGroup;
class GPUBuffer;
class DoubleSequenceOrGPUColorDict;
class GPURenderBundle;
class GPURenderPipeline;

class GPURenderPassEncoder : public DawnObject<WGPURenderPassEncoder>,
                             public GPUProgrammablePassEncoder {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPURenderPassEncoder(GPUDevice* device,
                                WGPURenderPassEncoder render_pass_encoder);
  ~GPURenderPassEncoder() override;

  // gpu_render_pass_encoder.idl
  void setBindGroup(uint32_t index,
                    GPUBindGroup* bindGroup,
                    const Vector<uint32_t>& dynamicOffsets);
  void setBindGroup(uint32_t index,
                    GPUBindGroup* bind_group,
                    const FlexibleUint32Array& dynamic_offsets_data,
                    uint64_t dynamic_offsets_data_start,
                    uint32_t dynamic_offsets_data_length,
                    ExceptionState& exception_state);
  void pushDebugGroup(String groupLabel);
  void popDebugGroup();
  void insertDebugMarker(String markerLabel);
  void setPipeline(GPURenderPipeline* pipeline);

  void setBlendColor(DoubleSequenceOrGPUColorDict& color,
                     ExceptionState& exception_state);
  void setStencilReference(uint32_t reference);
  void setViewport(float x,
                   float y,
                   float width,
                   float height,
                   float minDepth,
                   float maxDepth);
  void setScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
  void setIndexBuffer(GPUBuffer* buffer, uint64_t offset, uint64_t size);
  void setIndexBuffer(GPUBuffer* buffer,
                      const WTF::String& format,
                      uint64_t offset,
                      uint64_t size,
                      ExceptionState& exception_state);
  void setVertexBuffer(uint32_t slot,
                       const GPUBuffer* buffer,
                       const uint64_t offset,
                       const uint64_t size);
  void draw(uint32_t vertexCount,
            uint32_t instanceCount,
            uint32_t firstVertex,
            uint32_t firstInstance);
  void drawIndexed(uint32_t indexCount,
                   uint32_t instanceCount,
                   uint32_t firstIndex,
                   int32_t baseVertex,
                   uint32_t firstInstance);
  void drawIndirect(GPUBuffer* indirectBuffer, uint64_t indirectOffset);
  void drawIndexedIndirect(GPUBuffer* indirectBuffer, uint64_t indirectOffset);
  void executeBundles(const HeapVector<Member<GPURenderBundle>>& bundles);
  void endPass();

 private:
  DISALLOW_COPY_AND_ASSIGN(GPURenderPassEncoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PASS_ENCODER_H_
