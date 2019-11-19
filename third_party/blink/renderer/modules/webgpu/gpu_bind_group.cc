// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_sampler.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"

namespace blink {

WGPUBindGroupBinding AsDawnType(const GPUBindGroupBinding* webgpu_binding) {
  WGPUBindGroupBinding dawn_binding = {};

  dawn_binding.binding = webgpu_binding->binding();

  if (webgpu_binding->resource().IsGPUBufferBinding()) {
    GPUBufferBinding* buffer =
        webgpu_binding->resource().GetAsGPUBufferBinding();
    dawn_binding.offset = buffer->offset();
    dawn_binding.size = buffer->hasSize() ? buffer->size() : WGPU_WHOLE_SIZE;
    dawn_binding.buffer = AsDawnType(buffer->buffer());

  } else if (webgpu_binding->resource().IsGPUSampler()) {
    GPUSampler* sampler = webgpu_binding->resource().GetAsGPUSampler();
    dawn_binding.sampler = AsDawnType(sampler);

  } else if (webgpu_binding->resource().IsGPUTextureView()) {
    GPUTextureView* texture_view =
        webgpu_binding->resource().GetAsGPUTextureView();
    dawn_binding.textureView = AsDawnType(texture_view);

  } else {
    NOTREACHED();
  }

  return dawn_binding;
}

// static
GPUBindGroup* GPUBindGroup::Create(GPUDevice* device,
                                   const GPUBindGroupDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  uint32_t binding_count =
      static_cast<uint32_t>(webgpu_desc->bindings().size());

  std::unique_ptr<WGPUBindGroupBinding[]> bindings =
      binding_count != 0 ? AsDawnType(webgpu_desc->bindings()) : nullptr;

  WGPUBindGroupDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.layout = AsDawnType(webgpu_desc->layout());
  dawn_desc.bindingCount = binding_count;
  dawn_desc.bindings = bindings.get();
  if (webgpu_desc->hasLabel()) {
    dawn_desc.label = webgpu_desc->label().Utf8().data();
  }

  return MakeGarbageCollected<GPUBindGroup>(
      device, device->GetProcs().deviceCreateBindGroup(device->GetHandle(),
                                                       &dawn_desc));
}

GPUBindGroup::GPUBindGroup(GPUDevice* device, WGPUBindGroup bind_group)
    : DawnObject<WGPUBindGroup>(device, bind_group) {}

GPUBindGroup::~GPUBindGroup() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().bindGroupRelease(GetHandle());
}

}  // namespace blink
