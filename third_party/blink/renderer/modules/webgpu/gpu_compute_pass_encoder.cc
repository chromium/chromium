// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pass_encoder.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pipeline.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUComputePassEncoder* GPUComputePassEncoder::Create(
    GPUDevice* device,
    WGPUComputePassEncoder compute_pass_encoder) {
  return MakeGarbageCollected<GPUComputePassEncoder>(device,
                                                     compute_pass_encoder);
}

GPUComputePassEncoder::GPUComputePassEncoder(
    GPUDevice* device,
    WGPUComputePassEncoder compute_pass_encoder)
    : DawnObject<WGPUComputePassEncoder>(device, compute_pass_encoder) {}

GPUComputePassEncoder::~GPUComputePassEncoder() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().computePassEncoderRelease(GetHandle());
}

void GPUComputePassEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bindGroup,
    const Vector<uint32_t>& dynamicOffsets) {
  GetProcs().computePassEncoderSetBindGroup(
      GetHandle(), index, bindGroup->GetHandle(), dynamicOffsets.size(),
      dynamicOffsets.data());
}

void GPUComputePassEncoder::pushDebugGroup(String groupLabel) {
  GetProcs().computePassEncoderPushDebugGroup(GetHandle(),
                                              groupLabel.Utf8().data());
}

void GPUComputePassEncoder::popDebugGroup() {
  GetProcs().computePassEncoderPopDebugGroup(GetHandle());
}

void GPUComputePassEncoder::insertDebugMarker(String markerLabel) {
  GetProcs().computePassEncoderInsertDebugMarker(GetHandle(),
                                                 markerLabel.Utf8().data());
}

void GPUComputePassEncoder::setPipeline(GPUComputePipeline* pipeline) {
  GetProcs().computePassEncoderSetPipeline(GetHandle(), pipeline->GetHandle());
}

void GPUComputePassEncoder::dispatch(uint32_t x, uint32_t y, uint32_t z) {
  GetProcs().computePassEncoderDispatch(GetHandle(), x, y, z);
}

void GPUComputePassEncoder::dispatchIndirect(GPUBuffer* indirectBuffer,
                                             uint64_t indirectOffset) {
  GetProcs().computePassEncoderDispatchIndirect(
      GetHandle(), indirectBuffer->GetHandle(), indirectOffset);
}

void GPUComputePassEncoder::endPass() {
  GetProcs().computePassEncoderEndPass(GetHandle());
}

}  // namespace blink
