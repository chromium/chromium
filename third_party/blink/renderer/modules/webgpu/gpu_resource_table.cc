// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_resource_table.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_buffer_binding.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_resource_table_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpubuffer_gpubufferbinding_gpuexternaltexture_gpusampler_gputexture_gputextureview.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_sampler.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"

namespace blink {

namespace {

wgpu::BindingResource AsDawnType(const V8GPUBindingResource* webgpu_resource,
                                 ExceptionState& exception_state) {
  wgpu::BindingResource dawn_resource;

  switch (webgpu_resource->GetContentType()) {
    case V8GPUBindingResource::ContentType::kGPUBuffer: {
      GPUBuffer* buffer = webgpu_resource->GetAsGPUBuffer();
      dawn_resource.buffer = AsDawnType(buffer);
      break;
    }
    case V8GPUBindingResource::ContentType::kGPUBufferBinding: {
      GPUBufferBinding* buffer = webgpu_resource->GetAsGPUBufferBinding();
      dawn_resource.offset = buffer->offset();
      if (buffer->hasSize()) {
        dawn_resource.size = buffer->size();
      }
      dawn_resource.buffer = AsDawnType(buffer->buffer());
      break;
    }
    case V8GPUBindingResource::ContentType::kGPUSampler:
      dawn_resource.sampler = AsDawnType(webgpu_resource->GetAsGPUSampler());
      break;
    case V8GPUBindingResource::ContentType::kGPUTexture: {
      wgpu::Texture texture = AsDawnType(webgpu_resource->GetAsGPUTexture());
      dawn_resource.textureView = texture.CreateView();
      break;
    }
    case V8GPUBindingResource::ContentType::kGPUTextureView:
      dawn_resource.textureView =
          AsDawnType(webgpu_resource->GetAsGPUTextureView());
      break;
    case V8GPUBindingResource::ContentType::kGPUExternalTexture:
      exception_state.ThrowTypeError(
          "GPUExternalTexture used with GPUResourceTable");
      break;
  }

  return dawn_resource;
}

wgpu::ResourceTableDescriptor AsDawnType(
    const GPUResourceTableDescriptor* webgpu_desc,
    std::string* label) {
  DCHECK(webgpu_desc);
  DCHECK(label);

  wgpu::ResourceTableDescriptor dawn_desc = {
      .size = webgpu_desc->size(),
  };
  *label = webgpu_desc->label().Utf8();
  if (!label->empty()) {
    dawn_desc.label = label->c_str();
  }

  return dawn_desc;
}

}  // anonymous namespace

// static
GPUResourceTable* GPUResourceTable::Create(
    GPUDevice* device,
    const GPUResourceTableDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);
  std::string label;
  wgpu::ResourceTableDescriptor dawn_desc = AsDawnType(webgpu_desc, &label);
  GPUResourceTable* table = MakeGarbageCollected<GPUResourceTable>(
      device, device->GetHandle().CreateResourceTable(&dawn_desc),
      webgpu_desc->label());
  return table;
}

GPUResourceTable::GPUResourceTable(GPUDevice* device,
                                   wgpu::ResourceTable table,
                                   const String& label)
    : DawnObject<wgpu::ResourceTable>(device, std::move(table), label) {}

void GPUResourceTable::destroy() {
  GetHandle().Destroy();
}

void GPUResourceTable::update(uint32_t slot,
                              V8GPUBindingResource* resource,
                              ExceptionState& exception_state) {
  wgpu::BindingResource dawn_resource = AsDawnType(resource, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  if (GetHandle().Update(slot, &dawn_resource) != wgpu::Status::Success) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "update failed");
  }
}

uint32_t GPUResourceTable::insert(V8GPUBindingResource* resource,
                                  ExceptionState& exception_state) {
  wgpu::BindingResource dawn_resource = AsDawnType(resource, exception_state);
  if (exception_state.HadException()) {
    return 0;  // The exception means this value won't be seen by JS.
  }

  uint32_t slot = GetHandle().Insert(&dawn_resource);

  if (slot == wgpu::kInvalidBinding) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "insert failed");
  }

  return slot;
}

void GPUResourceTable::remove(uint32_t slot, ExceptionState& exception_state) {
  if (GetHandle().Remove(slot) != wgpu::Status::Success) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "remove failed");
  }
}

uint32_t GPUResourceTable::size() const {
  return GetHandle().GetSize();
}
}  // namespace blink
