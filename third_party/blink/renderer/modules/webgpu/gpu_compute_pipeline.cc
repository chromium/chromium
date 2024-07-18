// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pipeline.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pipeline_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_feature_name.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_programmable_stage.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_programmable_stage.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"

namespace blink {

wgpu::ComputePipelineDescriptor AsDawnType(
    GPUDevice* device,
    const GPUComputePipelineDescriptor* webgpu_desc,
    std::string* label,
    OwnedProgrammableStage* computeStage) {
  DCHECK(webgpu_desc);
  DCHECK(label);
  DCHECK(computeStage);

  wgpu::ComputePipelineDescriptor dawn_desc = {
      .layout = AsDawnType(webgpu_desc->layout()),
  };
  *label = webgpu_desc->label().Utf8();
  if (!label->empty()) {
    dawn_desc.label = label->c_str();
  }

  GPUProgrammableStage* programmable_stage_desc = webgpu_desc->compute();
  GPUProgrammableStageAsWGPUProgrammableStage(programmable_stage_desc,
                                              computeStage);
  dawn_desc.compute.constantCount = computeStage->constantCount;
  dawn_desc.compute.constants = computeStage->constants.get();
  dawn_desc.compute.module = programmable_stage_desc->module()->GetHandle();
  dawn_desc.compute.entryPoint =
      computeStage->entry_point ? computeStage->entry_point->c_str() : nullptr;

  return dawn_desc;
}

// static
GPUComputePipeline* GPUComputePipeline::Create(
    GPUDevice* device,
    const GPUComputePipelineDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  std::string label;
  OwnedProgrammableStage computeStage;
  wgpu::ComputePipelineDescriptor dawn_desc =
      AsDawnType(device, webgpu_desc, &label, &computeStage);

  // If ChromiumExperimentalSubgroups feature is enabled, chain the
  // full subgroups options after compute pipeline descriptor.
  // TODO(crbug.com/349125474): Remove deprecated ChromiumExperimentalSubgroups.
  wgpu::DawnComputePipelineFullSubgroups fullSubgroupsOptions = {};
  if (device->features()->has(
          V8GPUFeatureName::Enum::kChromiumExperimentalSubgroups)) {
    fullSubgroupsOptions.requiresFullSubgroups =
        webgpu_desc->getRequiresFullSubgroupsOr(false);
    dawn_desc.nextInChain = &fullSubgroupsOptions;
  }

  GPUComputePipeline* pipeline = MakeGarbageCollected<GPUComputePipeline>(
      device, device->GetHandle().CreateComputePipeline(&dawn_desc),
      webgpu_desc->label());
  return pipeline;
}

GPUComputePipeline::GPUComputePipeline(GPUDevice* device,
                                       wgpu::ComputePipeline compute_pipeline,
                                       const String& label)
    : DawnObject<wgpu::ComputePipeline>(device,
                                        std::move(compute_pipeline),
                                        label) {}

GPUBindGroupLayout* GPUComputePipeline::getBindGroupLayout(uint32_t index) {
  return MakeGarbageCollected<GPUBindGroupLayout>(
      device_, GetHandle().GetBindGroupLayout(index), String());
}

}  // namespace blink
