// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPUTE_PASS_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPUTE_PASS_ENCODER_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUBindGroup;
class GPUBuffer;
class GPUComputePipeline;

class GPUComputePassEncoder : public DawnObject<WGPUComputePassEncoder> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUComputePassEncoder* Create(
      GPUDevice* device,
      WGPUComputePassEncoder compute_pass_encoder);
  explicit GPUComputePassEncoder(GPUDevice* device,
                                 WGPUComputePassEncoder compute_pass_encoder);
  ~GPUComputePassEncoder() override;

  // gpu_compute_pass_encoder.idl
  void setBindGroup(uint32_t index,
                    GPUBindGroup* bindGroup,
                    const Vector<uint32_t>& dynamicOffsets);
  void pushDebugGroup(String groupLabel);
  void popDebugGroup();
  void insertDebugMarker(String markerLabel);
  void setPipeline(GPUComputePipeline* pipeline);
  void dispatch(uint32_t x, uint32_t y, uint32_t z);
  void dispatchIndirect(GPUBuffer* indirectBuffer, uint64_t indirectOffset);
  void endPass();

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUComputePassEncoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPUTE_PASS_ENCODER_H_
