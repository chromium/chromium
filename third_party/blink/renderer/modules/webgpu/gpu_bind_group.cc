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

WGPUBindGroupEntry AsDawnType(
    const GPUBindGroupEntry* webgpu_binding,
    Vector<std::unique_ptr<WGPUExternalTextureBindingEntry>>*
        externalTextureBindingEntries) {
  WGPUBindGroupEntry dawn_binding = {};

  dawn_binding.binding = webgpu_binding->binding();

  switch (webgpu_binding->resource()->GetContentType()) {
    case V8GPUBindingResource::ContentType::kGPUBufferBinding: {
      GPUBufferBinding* buffer =
          webgpu_binding->resource()->GetAsGPUBufferBinding();
      dawn_binding.offset = buffer->offset();
      dawn_binding.size = buffer->hasSize() ? buffer->size() : WGPU_WHOLE_SIZE;
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
      std::unique_ptr<WGPUExternalTextureBindingEntry>
          externalTextureBindingEntry =
              std::make_unique<WGPUExternalTextureBindingEntry>();
      externalTextureBindingEntry->externalTexture =
          AsDawnType(webgpu_binding->resource()->GetAsGPUExternalTexture());
      externalTextureBindingEntry->chain.sType =
          WGPUSType_ExternalTextureBindingEntry;
      dawn_binding.nextInChain = reinterpret_cast<WGPUChainedStruct*>(
          externalTextureBindingEntry.get());
      externalTextureBindingEntries->push_back(
          std::move(externalTextureBindingEntry));
      break;
  }

  return dawn_binding;
}

std::unique_ptr<WGPUBindGroupEntry[]> AsDawnType(
    const HeapVector<Member<GPUBindGroupEntry>>& webgpu_objects,
    Vector<std::unique_ptr<WGPUExternalTextureBindingEntry>>*
        externalTextureBindingEntries) {
  wtf_size_t count = webgpu_objects.size();
  std::unique_ptr<WGPUBindGroupEntry[]> dawn_objects(
      new WGPUBindGroupEntry[count]);
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
  std::unique_ptr<WGPUBindGroupEntry[]> entries;
  Vector<std::unique_ptr<WGPUExternalTextureBindingEntry>>
      externalTextureBindingEntries;
  entry_count = static_cast<uint32_t>(webgpu_desc->entries().size());
  if (entry_count > 0) {
    entries =
        AsDawnType(webgpu_desc->entries(), &externalTextureBindingEntries);
  }

  std::string label;
  WGPUBindGroupDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.layout = AsDawnType(webgpu_desc->layout());
  dawn_desc.entryCount = entry_count;
  dawn_desc.entries = entries.get();
  if (webgpu_desc->hasLabel()) {
    label = webgpu_desc->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  GPUBindGroup* bind_group = MakeGarbageCollected<GPUBindGroup>(
      device, device->GetProcs().deviceCreateBindGroup(device->GetHandle(),
                                                       &dawn_desc));
  if (webgpu_desc->hasLabel())
    bind_group->setLabel(webgpu_desc->label());
  return bind_group;
}

GPUBindGroup::GPUBindGroup(GPUDevice* device, WGPUBindGroup bind_group)
    : DawnObject<WGPUBindGroup>(device, bind_group) {}

}  // namespace blink
