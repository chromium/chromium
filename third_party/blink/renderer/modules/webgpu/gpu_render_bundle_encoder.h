// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_BUNDLE_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_BUNDLE_ENCODER_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_programmable_pass_encoder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class GPUBindGroup;
class GPUBuffer;
class GPURenderBundle;
class GPURenderBundleDescriptor;
class GPURenderBundleEncoderDescriptor;
class GPURenderPipeline;

class GPURenderBundleEncoder : public DawnObject<WGPURenderBundleEncoder>,
                               public GPUProgrammablePassEncoder {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPURenderBundleEncoder* Create(
      GPUDevice* device,
      const GPURenderBundleEncoderDescriptor* webgpu_desc);
  explicit GPURenderBundleEncoder(
      GPUDevice* device,
      WGPURenderBundleEncoder render_bundle_encoder);
  ~GPURenderBundleEncoder() override;

  // gpu_render_bundle_encoder.idl
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

  void setIndexBuffer(GPUBuffer* buffer, uint64_t offset, uint64_t size);
  void setIndexBuffer(GPUBuffer* buffer,
                      const WTF::String& format,
                      uint64_t offset,
                      uint64_t size,
                      ExceptionState& exception_state);
  void setVertexBuffer(uint32_t slot,
                       const GPUBuffer* buffer,
                       uint64_t offset,
                       uint64_t size);
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

  GPURenderBundle* finish(const GPURenderBundleDescriptor* webgpu_desc);

 private:
  DISALLOW_COPY_AND_ASSIGN(GPURenderBundleEncoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_BUNDLE_ENCODER_H_
