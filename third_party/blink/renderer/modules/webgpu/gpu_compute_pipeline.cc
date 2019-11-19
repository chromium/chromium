// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pipeline.h"

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pipeline_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_programmable_stage_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"

namespace blink {

// static
GPUComputePipeline* GPUComputePipeline::Create(
    GPUDevice* device,
    const GPUComputePipelineDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  WGPUComputePipelineDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.layout = AsDawnType(webgpu_desc->layout());
  if (webgpu_desc->hasLabel()) {
    dawn_desc.label = webgpu_desc->label().Utf8().data();
  }

  auto compute_stage = AsDawnType(webgpu_desc->computeStage());
  dawn_desc.computeStage = std::get<0>(compute_stage);

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

}  // namespace blink
