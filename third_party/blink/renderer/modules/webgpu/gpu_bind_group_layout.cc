// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_bind_group_layout_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_bind_group_layout_entry.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_buffer_binding_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_sampler_binding_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_storage_texture_binding_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_binding_layout.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

WGPUBindGroupLayoutEntry AsDawnType(
    const GPUBindGroupLayoutEntry* webgpu_binding,
    GPUDevice* device) {
  WGPUBindGroupLayoutEntry dawn_binding = {};

  dawn_binding.binding = webgpu_binding->binding();
  dawn_binding.visibility =
      AsDawnEnum<WGPUShaderStage>(webgpu_binding->visibility());

  if (webgpu_binding->hasBuffer()) {
    dawn_binding.buffer.type =
        AsDawnEnum<WGPUBufferBindingType>(webgpu_binding->buffer()->type());
    dawn_binding.buffer.hasDynamicOffset =
        webgpu_binding->buffer()->hasDynamicOffset();
    dawn_binding.buffer.minBindingSize =
        webgpu_binding->buffer()->minBindingSize();
  }

  if (webgpu_binding->hasSampler()) {
    dawn_binding.sampler.type =
        AsDawnEnum<WGPUSamplerBindingType>(webgpu_binding->sampler()->type());
  }

  if (webgpu_binding->hasTexture()) {
    dawn_binding.texture.sampleType = AsDawnEnum<WGPUTextureSampleType>(
        webgpu_binding->texture()->sampleType());
    dawn_binding.texture.viewDimension = AsDawnEnum<WGPUTextureViewDimension>(
        webgpu_binding->texture()->viewDimension());
    dawn_binding.texture.multisampled =
        webgpu_binding->texture()->multisampled();
  }

  if (webgpu_binding->hasStorageTexture()) {
    dawn_binding.storageTexture.access = AsDawnEnum<WGPUStorageTextureAccess>(
        webgpu_binding->storageTexture()->access());
    dawn_binding.storageTexture.format = AsDawnEnum<WGPUTextureFormat>(
        webgpu_binding->storageTexture()->format());
    dawn_binding.storageTexture.viewDimension =
        AsDawnEnum<WGPUTextureViewDimension>(
            webgpu_binding->storageTexture()->viewDimension());
  }

  // Deprecated values
  if (webgpu_binding->hasType()) {
    device->AddConsoleWarning(
        "The format of GPUBindGroupLayoutEntry has changed, and will soon "
        "require the buffer, sampler, texture, or storageTexture members be "
        "set rather than setting type, etc. on the entry directly.");

    dawn_binding.type = AsDawnEnum<WGPUBindingType>(webgpu_binding->type());

    dawn_binding.hasDynamicOffset = webgpu_binding->hasDynamicOffset();

    dawn_binding.minBufferBindingSize =
        webgpu_binding->hasMinBufferBindingSize()
            ? webgpu_binding->minBufferBindingSize()
            : 0;

    dawn_binding.viewDimension =
        AsDawnEnum<WGPUTextureViewDimension>(webgpu_binding->viewDimension());

    dawn_binding.textureComponentType = AsDawnEnum<WGPUTextureComponentType>(
        webgpu_binding->textureComponentType());

    dawn_binding.storageTextureFormat =
        AsDawnEnum<WGPUTextureFormat>(webgpu_binding->storageTextureFormat());
  }

  return dawn_binding;
}

// TODO(crbug.com/1069302): Remove when unused.
std::unique_ptr<WGPUBindGroupLayoutEntry[]> AsDawnType(
    const HeapVector<Member<GPUBindGroupLayoutEntry>>& webgpu_objects,
    GPUDevice* device) {
  wtf_size_t count = webgpu_objects.size();
  std::unique_ptr<WGPUBindGroupLayoutEntry[]> dawn_objects(
      new WGPUBindGroupLayoutEntry[count]);
  for (wtf_size_t i = 0; i < count; ++i) {
    dawn_objects[i] = AsDawnType(webgpu_objects[i].Get(), device);
  }
  return dawn_objects;
}

// static
GPUBindGroupLayout* GPUBindGroupLayout::Create(
    GPUDevice* device,
    const GPUBindGroupLayoutDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  uint32_t entry_count = 0;
  std::unique_ptr<WGPUBindGroupLayoutEntry[]> entries;
  entry_count = static_cast<uint32_t>(webgpu_desc->entries().size());
  if (entry_count > 0) {
    entries = AsDawnType(webgpu_desc->entries(), device);
  }

  std::string label;
  WGPUBindGroupLayoutDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.entryCount = entry_count;
  dawn_desc.entries = entries.get();
  if (webgpu_desc->hasLabel()) {
    label = webgpu_desc->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  return MakeGarbageCollected<GPUBindGroupLayout>(
      device, device->GetProcs().deviceCreateBindGroupLayout(
                  device->GetHandle(), &dawn_desc));
}

GPUBindGroupLayout::GPUBindGroupLayout(GPUDevice* device,
                                       WGPUBindGroupLayout bind_group_layout)
    : DawnObject<WGPUBindGroupLayout>(device, bind_group_layout) {}

GPUBindGroupLayout::~GPUBindGroupLayout() {
  GetProcs().bindGroupLayoutRelease(GetHandle());
}

}  // namespace blink
