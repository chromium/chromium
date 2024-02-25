// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_bind_group_layout_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_bind_group_layout_entry.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_buffer_binding_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_external_texture_binding_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_feature_name.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_sampler_binding_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_storage_texture_binding_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_binding_layout.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

WGPUBindGroupLayoutEntry AsDawnType(
    GPUDevice* device,
    const GPUBindGroupLayoutEntry* webgpu_binding,
    Vector<std::unique_ptr<WGPUExternalTextureBindingLayout>>*
        externalTextureBindingLayouts,
    ExceptionState& exception_state) {
  WGPUBindGroupLayoutEntry dawn_binding = {};

  dawn_binding.binding = webgpu_binding->binding();
  dawn_binding.visibility =
      AsDawnFlags<WGPUShaderStage>(webgpu_binding->visibility());

  if (webgpu_binding->hasBuffer()) {
    dawn_binding.buffer.type = AsDawnEnum(webgpu_binding->buffer()->type());
    dawn_binding.buffer.hasDynamicOffset =
        webgpu_binding->buffer()->hasDynamicOffset();
    dawn_binding.buffer.minBindingSize =
        webgpu_binding->buffer()->minBindingSize();
  }

  if (webgpu_binding->hasSampler()) {
    dawn_binding.sampler.type = AsDawnEnum(webgpu_binding->sampler()->type());
  }

  if (webgpu_binding->hasTexture()) {
    dawn_binding.texture.sampleType =
        AsDawnEnum(webgpu_binding->texture()->sampleType());
    dawn_binding.texture.viewDimension =
        AsDawnEnum(webgpu_binding->texture()->viewDimension());
    dawn_binding.texture.multisampled =
        webgpu_binding->texture()->multisampled();
  }

  if (webgpu_binding->hasStorageTexture()) {
    if (!device->ValidateTextureFormatUsage(
            webgpu_binding->storageTexture()->format(), exception_state)) {
      return {};
    }

    dawn_binding.storageTexture.access =
        AsDawnEnum(webgpu_binding->storageTexture()->access());
    dawn_binding.storageTexture.format =
        AsDawnEnum(webgpu_binding->storageTexture()->format());
    dawn_binding.storageTexture.viewDimension =
        AsDawnEnum(webgpu_binding->storageTexture()->viewDimension());
  }

  if (webgpu_binding->hasExternalTexture()) {
    std::unique_ptr<WGPUExternalTextureBindingLayout>
        externalTextureBindingLayout =
            std::make_unique<WGPUExternalTextureBindingLayout>();
    externalTextureBindingLayout->chain.sType =
        WGPUSType_ExternalTextureBindingLayout;
    dawn_binding.nextInChain = reinterpret_cast<WGPUChainedStruct*>(
        externalTextureBindingLayout.get());
    externalTextureBindingLayouts->push_back(
        std::move(externalTextureBindingLayout));
  }

  return dawn_binding;
}

// TODO(crbug.com/1069302): Remove when unused.
std::unique_ptr<WGPUBindGroupLayoutEntry[]> AsDawnType(
    GPUDevice* device,
    const HeapVector<Member<GPUBindGroupLayoutEntry>>& webgpu_objects,
    Vector<std::unique_ptr<WGPUExternalTextureBindingLayout>>*
        externalTextureBindingLayouts,
    ExceptionState& exception_state) {
  wtf_size_t count = webgpu_objects.size();
  std::unique_ptr<WGPUBindGroupLayoutEntry[]> dawn_objects(
      new WGPUBindGroupLayoutEntry[count]);
  for (wtf_size_t i = 0; i < count; ++i) {
    dawn_objects[i] =
        AsDawnType(device, webgpu_objects[i].Get(),
                   externalTextureBindingLayouts, exception_state);
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
  Vector<std::unique_ptr<WGPUExternalTextureBindingLayout>>
      externalTextureBindingLayouts;
  entry_count = static_cast<uint32_t>(webgpu_desc->entries().size());
  if (entry_count > 0) {
    entries = AsDawnType(device, webgpu_desc->entries(),
                         &externalTextureBindingLayouts, exception_state);
  }

  if (exception_state.HadException()) {
    return nullptr;
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

  GPUBindGroupLayout* layout = MakeGarbageCollected<GPUBindGroupLayout>(
      device, device->GetProcs().deviceCreateBindGroupLayout(
                  device->GetHandle(), &dawn_desc));
  if (webgpu_desc->hasLabel())
    layout->setLabel(webgpu_desc->label());
  return layout;
}

GPUBindGroupLayout::GPUBindGroupLayout(GPUDevice* device,
                                       WGPUBindGroupLayout bind_group_layout)
    : DawnObject<WGPUBindGroupLayout>(device, bind_group_layout) {}

}  // namespace blink
