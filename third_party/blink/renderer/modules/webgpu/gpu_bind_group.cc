// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_bind_group_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_bind_group_entry.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_buffer_binding.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpubufferbinding_gpuexternaltexture_gpusampler_gputextureview.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_external_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_sampler.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

wgpu::BindGroupEntry AsDawnType(
    const GPUBindGroupEntry* webgpu_binding,
    Vector<std::unique_ptr<wgpu::ExternalTextureBindingEntry>>*
        externalTextureBindingEntries) {
  wgpu::BindGroupEntry dawn_binding = {
      .binding = webgpu_binding->binding(),
  };

  switch (webgpu_binding->resource()->GetContentType()) {
    case V8GPUBindingResource::ContentType::kGPUBufferBinding: {
      GPUBufferBinding* buffer =
          webgpu_binding->resource()->GetAsGPUBufferBinding();
      dawn_binding.offset = buffer->offset();
      if (buffer->hasSize()) {
        dawn_binding.size = buffer->size();
      }
      dawn_binding.buffer = AsDawnType(buffer->buffer());
      break;
    }
    case V8GPUBindingResource::ContentType::kGPUSampler:
      dawn_binding.sampler =
          AsDawnType(webgpu_binding->resource()->GetAsGPUSampler());
      break;
    case V8GPUBindingResource::ContentType::kGPUTextureView:
      dawn_binding.textureView =
          AsDawnType(webgpu_binding->resource()->GetAsGPUTextureView());
      break;
    case V8GPUBindingResource::ContentType::kGPUExternalTexture:
      std::unique_ptr<wgpu::ExternalTextureBindingEntry>
          externalTextureBindingEntry =
              std::make_unique<wgpu::ExternalTextureBindingEntry>();
      externalTextureBindingEntry->externalTexture =
          AsDawnType(webgpu_binding->resource()->GetAsGPUExternalTexture());
      dawn_binding.nextInChain = externalTextureBindingEntry.get();
      externalTextureBindingEntries->push_back(
          std::move(externalTextureBindingEntry));
      break;
  }

  return dawn_binding;
}

std::unique_ptr<wgpu::BindGroupEntry[]> AsDawnType(
    const HeapVector<Member<GPUBindGroupEntry>>& webgpu_objects,
    Vector<std::unique_ptr<wgpu::ExternalTextureBindingEntry>>*
        externalTextureBindingEntries) {
  wtf_size_t count = webgpu_objects.size();
  std::unique_ptr<wgpu::BindGroupEntry[]> dawn_objects(
      new wgpu::BindGroupEntry[count]);
  for (wtf_size_t i = 0; i < count; ++i) {
    dawn_objects[i] =
        AsDawnType(webgpu_objects[i].Get(), externalTextureBindingEntries);
  }
  return dawn_objects;
}

// static
GPUBindGroup* GPUBindGroup::Create(GPUDevice* device,
                                   const GPUBindGroupDescriptor* webgpu_desc,
                                   ExceptionState& exception_state) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  uint32_t entry_count = 0;
  std::unique_ptr<wgpu::BindGroupEntry[]> entries;
  Vector<std::unique_ptr<wgpu::ExternalTextureBindingEntry>>
      externalTextureBindingEntries;
  entry_count = static_cast<uint32_t>(webgpu_desc->entries().size());
  if (entry_count > 0) {
    entries =
        AsDawnType(webgpu_desc->entries(), &externalTextureBindingEntries);
  }

  wgpu::BindGroupDescriptor dawn_desc = {
      .layout = AsDawnType(webgpu_desc->layout()),
      .entryCount = entry_count,
      .entries = entries.get(),
  };
  std::string label = webgpu_desc->label().Utf8();
  if (!label.empty()) {
    dawn_desc.label = label.c_str();
  }

  GPUBindGroup* bind_group = MakeGarbageCollected<GPUBindGroup>(
      device, device->GetHandle().CreateBindGroup(&dawn_desc),
      webgpu_desc->label());
  return bind_group;
}

GPUBindGroup::GPUBindGroup(GPUDevice* device,
                           wgpu::BindGroup bind_group,
                           const String& label)
    : DawnObject<wgpu::BindGroup>(device, std::move(bind_group), label) {}

}  // namespace blink
