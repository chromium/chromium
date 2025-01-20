// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_pipeline_layout_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
GPUPipelineLayout* GPUPipelineLayout::Create(
    ScriptState* script_state,
    GPUDevice* device,
    const GPUPipelineLayoutDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  v8::Isolate* isolate = script_state->GetIsolate();
  size_t bind_group_layout_count = webgpu_desc->bindGroupLayouts().size();

  std::unique_ptr<wgpu::BindGroupLayout[]> bind_group_layouts =
      bind_group_layout_count != 0 ? AsDawnType(webgpu_desc->bindGroupLayouts())
                                   : nullptr;
  // TODO(crbug.com/377836524): Remove WebGPUAllowNullInPipelineLayoutEntries
  // and the check here once the feature is safely landed.
  if (!RuntimeEnabledFeatures::
          WebGPUAllowNullInPipelineLayoutEntriesEnabled()) {
    for (const auto& bind_group_layout : webgpu_desc->bindGroupLayouts()) {
      if (bind_group_layout == nullptr) {
        ExceptionState exception_state(isolate);
        exception_state.ThrowTypeError(
            "Null bind group layout in PipelineLayoutDescriptor requires the "
            "WebGPUAllowNullInPipelineLayoutEntries Blink feature.");
        return nullptr;
      }
    }
  }

  wgpu::PipelineLayoutDescriptor dawn_desc = {
      .bindGroupLayoutCount = bind_group_layout_count,
      .bindGroupLayouts = bind_group_layouts.get(),
  };
  std::string label = webgpu_desc->label().Utf8();
  if (!label.empty()) {
    dawn_desc.label = label.c_str();
  }

  GPUPipelineLayout* layout = MakeGarbageCollected<GPUPipelineLayout>(
      device, device->GetHandle().CreatePipelineLayout(&dawn_desc),
      webgpu_desc->label());
  return layout;
}

GPUPipelineLayout::GPUPipelineLayout(GPUDevice* device,
                                     wgpu::PipelineLayout pipeline_layout,
                                     const String& label)
    : DawnObject<wgpu::PipelineLayout>(device,
                                       std::move(pipeline_layout),
                                       label) {}

}  // namespace blink
