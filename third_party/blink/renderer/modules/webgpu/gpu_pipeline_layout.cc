// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"

#include <string>

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_pipeline_layout_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

namespace {

struct OwnedPipelineLayoutDescriptor {
  OwnedPipelineLayoutDescriptor() = default;

  //  This struct should be non-copyable non-movable because it contains
  //  self-referencing pointers that would be invalidated when moved / copied.
  OwnedPipelineLayoutDescriptor(const OwnedPipelineLayoutDescriptor& desc) =
      delete;
  OwnedPipelineLayoutDescriptor(OwnedPipelineLayoutDescriptor&& desc) = delete;
  OwnedPipelineLayoutDescriptor& operator=(
      const OwnedPipelineLayoutDescriptor& desc) = delete;
  OwnedPipelineLayoutDescriptor& operator=(
      OwnedPipelineLayoutDescriptor&& desc) = delete;

  wgpu::PipelineLayoutDescriptor dawn_desc = {};
  std::unique_ptr<wgpu::BindGroupLayout[]> bind_group_layouts;
  std::string label;
  wgpu::PipelineLayoutResourceTable resource_table_desc;
};

void ConvertToDawnType(const GPUPipelineLayoutDescriptor* webgpu_desc,
                       OwnedPipelineLayoutDescriptor* owned_dawn_desc) {
  DCHECK(webgpu_desc);
  DCHECK(owned_dawn_desc);

  if (!webgpu_desc->bindGroupLayouts().empty()) {
    owned_dawn_desc->bind_group_layouts =
        AsDawnType(webgpu_desc->bindGroupLayouts());

    owned_dawn_desc->dawn_desc.bindGroupLayoutCount =
        webgpu_desc->bindGroupLayouts().size();
    owned_dawn_desc->dawn_desc.bindGroupLayouts =
        owned_dawn_desc->bind_group_layouts.get();
  }

  owned_dawn_desc->dawn_desc.immediateSize = webgpu_desc->immediateSize();

  if (!webgpu_desc->label().empty()) {
    owned_dawn_desc->label = webgpu_desc->label().Utf8();
    owned_dawn_desc->dawn_desc.label = owned_dawn_desc->label.c_str();
  }

  if (webgpu_desc->usesResourceTable()) {
    owned_dawn_desc->resource_table_desc.usesResourceTable = true;
    owned_dawn_desc->resource_table_desc.nextInChain =
        owned_dawn_desc->dawn_desc.nextInChain;
    owned_dawn_desc->dawn_desc.nextInChain =
        &owned_dawn_desc->resource_table_desc;
  }
}

}  // anonymous namespace

// static
GPUPipelineLayout* GPUPipelineLayout::Create(
    GPUDevice* device,
    const GPUPipelineLayoutDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  OwnedPipelineLayoutDescriptor owned_dawn_desc;
  ConvertToDawnType(webgpu_desc, &owned_dawn_desc);

  GPUPipelineLayout* layout = MakeGarbageCollected<GPUPipelineLayout>(
      device,
      device->GetHandle().CreatePipelineLayout(&owned_dawn_desc.dawn_desc),
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
