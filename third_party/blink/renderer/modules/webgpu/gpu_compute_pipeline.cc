// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pipeline.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pipeline_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_programmable_stage_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"

namespace blink {

WGPUComputePipelineDescriptor AsDawnType(
    const GPUComputePipelineDescriptor* webgpu_desc,
    std::string* label,
    OwnedProgrammableStageDescriptor* computeStageDescriptor) {
  DCHECK(webgpu_desc);
  DCHECK(label);
  DCHECK(computeStageDescriptor);

  WGPUComputePipelineDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  if (webgpu_desc->hasLayout()) {
    dawn_desc.layout = AsDawnType(webgpu_desc->layout());
  }
  if (webgpu_desc->hasLabel()) {
    *label = webgpu_desc->label().Utf8();
    dawn_desc.label = label->c_str();
  }

  *computeStageDescriptor = AsDawnType(webgpu_desc->computeStage());
  dawn_desc.computeStage = std::get<0>(*computeStageDescriptor);

  return dawn_desc;
}

// static
GPUComputePipeline* GPUComputePipeline::Create(
    GPUDevice* device,
    const GPUComputePipelineDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  std::string label;
  OwnedProgrammableStageDescriptor computeStageDescriptor;
  WGPUComputePipelineDescriptor dawn_desc =
      AsDawnType(webgpu_desc, &label, &computeStageDescriptor);

  return MakeGarbageCollected<GPUComputePipeline>(
      device, device->GetProcs().deviceCreateComputePipeline(
                  device->GetHandle(), &dawn_desc));
}

GPUComputePipeline::GPUComputePipeline(GPUDevice* device,
                                       WGPUComputePipeline compute_pipeline)
    : DawnObject<WGPUComputePipeline>(device, compute_pipeline) {}

GPUComputePipeline::~GPUComputePipeline() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().computePipelineRelease(GetHandle());
}

GPUBindGroupLayout* GPUComputePipeline::getBindGroupLayout(uint32_t index) {
  return MakeGarbageCollected<GPUBindGroupLayout>(
      device_,
      GetProcs().computePipelineGetBindGroupLayout(GetHandle(), index));
}

}  // namespace blink
