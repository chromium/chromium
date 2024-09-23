// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_adapter_info.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_memory_heap_info.h"

namespace blink {

GPUAdapterInfo::GPUAdapterInfo(const String& vendor,
                               const String& architecture,
                               const String& device,
                               const String& description,
                               const String& driver,
                               const String& backend,
                               const String& type,
                               const std::optional<uint32_t> d3d_shader_model,
                               const std::optional<uint32_t> vk_driver_version)
    : vendor_(vendor),
      architecture_(architecture),
      device_(device),
      description_(description),
      driver_(driver),
      backend_(backend),
      type_(type),
      d3d_shader_model_(d3d_shader_model),
      vk_driver_version_(vk_driver_version) {}

void GPUAdapterInfo::AppendMemoryHeapInfo(GPUMemoryHeapInfo* info) {
  memory_heaps_.push_back(info);
}

const String& GPUAdapterInfo::vendor() const {
  return vendor_;
}

const String& GPUAdapterInfo::architecture() const {
  return architecture_;
}

const String& GPUAdapterInfo::device() const {
  return device_;
}

const String& GPUAdapterInfo::description() const {
  return description_;
}

const String& GPUAdapterInfo::driver() const {
  return driver_;
}

const String& GPUAdapterInfo::backend() const {
  return backend_;
}

const String& GPUAdapterInfo::type() const {
  return type_;
}

const HeapVector<Member<GPUMemoryHeapInfo>>& GPUAdapterInfo::memoryHeaps()
    const {
  return memory_heaps_;
}

const std::optional<uint32_t>& GPUAdapterInfo::d3dShaderModel() const {
  return d3d_shader_model_;
}

const std::optional<uint32_t>& GPUAdapterInfo::vkDriverVersion() const {
  return vk_driver_version_;
}

void GPUAdapterInfo::Trace(Visitor* visitor) const {
  visitor->Trace(memory_heaps_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
