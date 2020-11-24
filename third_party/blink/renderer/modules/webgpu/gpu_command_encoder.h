// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class ExceptionState;
class GPUBuffer;
class GPUBufferCopyView;
class GPUCommandBuffer;
class GPUCommandBufferDescriptor;
class GPUCommandEncoderDescriptor;
class GPUComputePassDescriptor;
class GPUComputePassEncoder;
class GPUQuerySet;
class GPURenderPassDescriptor;
class GPURenderPassEncoder;
class GPUTextureCopyView;
class UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict;

class GPUCommandEncoder : public DawnObject<WGPUCommandEncoder> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUCommandEncoder* Create(
      GPUDevice* device,
      const GPUCommandEncoderDescriptor* webgpu_desc);
  explicit GPUCommandEncoder(GPUDevice* device,
                             WGPUCommandEncoder command_encoder);
  ~GPUCommandEncoder() override;

  // gpu_command_encoder.idl
  GPURenderPassEncoder* beginRenderPass(
      const GPURenderPassDescriptor* descriptor,
      ExceptionState& exception_state);
  GPUComputePassEncoder* beginComputePass(
      const GPUComputePassDescriptor* descriptor);
  void copyBufferToBuffer(GPUBuffer* src,
                          uint64_t src_offset,
                          GPUBuffer* dst,
                          uint64_t dst_offset,
                          uint64_t size);
  void copyBufferToTexture(
      GPUBufferCopyView* source,
      GPUTextureCopyView* destination,
      UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& copy_size);
  void copyTextureToBuffer(
      GPUTextureCopyView* source,
      GPUBufferCopyView* destination,
      UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& copy_size);
  void copyTextureToTexture(
      GPUTextureCopyView* source,
      GPUTextureCopyView* destination,
      UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& copy_size);
  void pushDebugGroup(String groupLabel);
  void popDebugGroup();
  void insertDebugMarker(String markerLabel);
  void resolveQuerySet(GPUQuerySet* querySet,
                       uint32_t firstQuery,
                       uint32_t queryCount,
                       GPUBuffer* destination,
                       uint64_t destinationOffset);
  void writeTimestamp(GPUQuerySet* querySet, uint32_t queryIndex);
  GPUCommandBuffer* finish(const GPUCommandBufferDescriptor* descriptor);

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUCommandEncoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_
