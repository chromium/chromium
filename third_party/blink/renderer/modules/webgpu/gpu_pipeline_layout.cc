// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout_descriptor.h"

namespace blink {

// static
GPUPipelineLayout* GPUPipelineLayout::Create(
    GPUDevice* device,
    const GPUPipelineLayoutDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  uint32_t bind_group_layout_count =
      static_cast<uint32_t>(webgpu_desc->bindGroupLayouts().size());

  std::unique_ptr<WGPUBindGroupLayout[]> bind_group_layouts =
      bind_group_layout_count != 0 ? AsDawnType(webgpu_desc->bindGroupLayouts())
                                   : nullptr;

  WGPUPipelineLayoutDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.bindGroupLayoutCount = bind_group_layout_count;
  dawn_desc.bindGroupLayouts = bind_group_layouts.get();
  if (webgpu_desc->hasLabel()) {
    dawn_desc.label = webgpu_desc->label().Utf8().data();
  }

  return MakeGarbageCollected<GPUPipelineLayout>(
      device, device->GetProcs().deviceCreatePipelineLayout(device->GetHandle(),
                                                            &dawn_desc));
}

GPUPipelineLayout::GPUPipelineLayout(GPUDevice* device,
                                     WGPUPipelineLayout pipeline_layout)
    : DawnObject<WGPUPipelineLayout>(device, pipeline_layout) {}

GPUPipelineLayout::~GPUPipelineLayout() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().pipelineLayoutRelease(GetHandle());
}

}  // namespace blink
