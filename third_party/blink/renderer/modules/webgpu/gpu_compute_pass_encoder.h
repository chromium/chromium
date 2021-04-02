// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPUTE_PASS_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPUTE_PASS_ENCODER_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_programmable_pass_encoder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class GPUBindGroup;

class GPUComputePassEncoder : public DawnObject<WGPUComputePassEncoder>,
                              public GPUProgrammablePassEncoder {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUComputePassEncoder(GPUDevice* device,
                                 WGPUComputePassEncoder compute_pass_encoder);

  // gpu_compute_pass_encoder.idl
  void setBindGroup(uint32_t index,
                    const DawnObject<WGPUBindGroup>* bindGroup) {
    GetProcs().computePassEncoderSetBindGroup(
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
    GetProcs().computePassEncoderPushDebugGroup(GetHandle(), label.c_str());
  }
  void popDebugGroup() {
    GetProcs().computePassEncoderPopDebugGroup(GetHandle());
  }
  void insertDebugMarker(String markerLabel) {
    std::string label = markerLabel.Utf8();
    GetProcs().computePassEncoderInsertDebugMarker(GetHandle(), label.c_str());
  }
  void setPipeline(const DawnObject<WGPUComputePipeline>* pipeline) {
    GetProcs().computePassEncoderSetPipeline(GetHandle(),
                                             pipeline->GetHandle());
  }
  void dispatch(uint32_t x, uint32_t y, uint32_t z) {
    GetProcs().computePassEncoderDispatch(GetHandle(), x, y, z);
  }
  void dispatchIndirect(const DawnObject<WGPUBuffer>* indirectBuffer,
                        uint64_t indirectOffset) {
    GetProcs().computePassEncoderDispatchIndirect(
        GetHandle(), indirectBuffer->GetHandle(), indirectOffset);
  }
  void writeTimestamp(const DawnObject<WGPUQuerySet>* querySet,
                      uint32_t queryIndex) {
    GetProcs().computePassEncoderWriteTimestamp(
        GetHandle(), querySet->GetHandle(), queryIndex);
  }
  void endPass() { GetProcs().computePassEncoderEndPass(GetHandle()); }

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUComputePassEncoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPUTE_PASS_ENCODER_H_
