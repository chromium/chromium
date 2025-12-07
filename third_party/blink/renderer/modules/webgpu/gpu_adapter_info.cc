// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_adapter_info.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_memory_heap_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_subgroup_matrix_config.h"

namespace blink {

GPUAdapterInfo::GPUAdapterInfo(const String& vendor,
                               const String& architecture,
                               uint32_t subgroup_min_size,
                               uint32_t subgroup_max_size,
                               bool is_fallback_adapter,
                               const String& device,
                               const String& description,
                               const String& driver,
                               const String& backend,
                               const String& type,
                               const std::optional<uint32_t> d3d_shader_model,
                               const std::optional<uint32_t> vk_driver_version,
                               const String& power_preference)
    : vendor_(vendor),
      architecture_(architecture),
      subgroup_min_size_(subgroup_min_size),
      subgroup_max_size_(subgroup_max_size),
      is_fallback_adapter_(is_fallback_adapter),
      device_(device),
      description_(description),
      driver_(driver),
      backend_(backend),
      type_(type),
      d3d_shader_model_(d3d_shader_model),
      vk_driver_version_(vk_driver_version),
      power_preference_(power_preference) {}

void GPUAdapterInfo::AppendMemoryHeapInfo(GPUMemoryHeapInfo* info) {
  memory_heaps_.push_back(info);
}

void GPUAdapterInfo::AppendSubgroupMatrixConfig(
    GPUSubgroupMatrixConfig* config) {
  subgroup_matrix_configs_.push_back(config);
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

uint32_t GPUAdapterInfo::subgroupMinSize() const {
  return subgroup_min_size_;
}

uint32_t GPUAdapterInfo::subgroupMaxSize() const {
  return subgroup_max_size_;
}

bool GPUAdapterInfo::isFallbackAdapter() const {
  return is_fallback_adapter_;
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

const HeapVector<Member<GPUSubgroupMatrixConfig>>&
GPUAdapterInfo::subgroupMatrixConfigs() const {
  return subgroup_matrix_configs_;
}

const std::optional<uint32_t>& GPUAdapterInfo::d3dShaderModel() const {
  return d3d_shader_model_;
}

const std::optional<uint32_t>& GPUAdapterInfo::vkDriverVersion() const {
  return vk_driver_version_;
}

const String& GPUAdapterInfo::powerPreference() const {
  return power_preference_;
}

void GPUAdapterInfo::Trace(Visitor* visitor) const {
  visitor->Trace(memory_heaps_);
  visitor->Trace(subgroup_matrix_configs_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
