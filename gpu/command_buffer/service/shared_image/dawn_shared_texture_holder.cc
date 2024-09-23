// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_shared_texture_holder.h"

#include "base/check_op.h"

namespace gpu {

DawnSharedTextureHolder::SharedTextureData::SharedTextureData() = default;

DawnSharedTextureHolder::SharedTextureData::~SharedTextureData() {
  for (auto [texture_usage, texture] : texture_cache) {
    texture.Destroy();
  }
}

DawnSharedTextureHolder::SharedTextureData::SharedTextureData(
    SharedTextureData&&) = default;

DawnSharedTextureHolder::SharedTextureData&
DawnSharedTextureHolder::SharedTextureData::operator=(SharedTextureData&&) =
    default;

DawnSharedTextureHolder::DawnSharedTextureHolder() = default;

DawnSharedTextureHolder::~DawnSharedTextureHolder() {}

DawnSharedTextureHolder::DawnSharedTextureHolder(DawnSharedTextureHolder&&) =
    default;

DawnSharedTextureHolder& DawnSharedTextureHolder::operator=(
    DawnSharedTextureHolder&&) = default;

DawnSharedTextureHolder::WGPUTextureCache*
DawnSharedTextureHolder::GetWGPUTextureCache(const wgpu::Device& device) {
  auto iter = shared_texture_data_cache_.find(device.Get());
  if (iter == shared_texture_data_cache_.end()) {
    return nullptr;
  }
  return &iter->second.texture_cache;
}

wgpu::SharedTextureMemory DawnSharedTextureHolder::GetSharedTextureMemory(
    const wgpu::Device& device) {
  auto iter = shared_texture_data_cache_.find(device.Get());
  if (iter == shared_texture_data_cache_.end()) {
    return nullptr;
  }
  return iter->second.memory;
}

void DawnSharedTextureHolder::MaybeCacheSharedTextureMemory(
    const wgpu::Device& device,
    const wgpu::SharedTextureMemory& memory) {
  if (!memory) {
    return;
  }

  // Return early if the STM is already cached.
  if (auto cached = GetSharedTextureMemory(device)) {
    CHECK_EQ(cached.Get(), memory.Get());
    return;
  }

  SharedTextureData shared_texture_data;
  shared_texture_data.memory = memory;
  shared_texture_data_cache_.emplace(device.Get(),
                                     std::move(shared_texture_data));
}

wgpu::Texture DawnSharedTextureHolder::GetCachedWGPUTexture(
    const wgpu::Device& device,
    wgpu::TextureUsage texture_usage) {
  auto* texture_cache = GetWGPUTextureCache(device);
  if (!texture_cache) {
    return nullptr;
  }

  auto iter = texture_cache->find(texture_usage);
  if (iter == texture_cache->end()) {
    return nullptr;
  }

  return iter->second;
}

void DawnSharedTextureHolder::RemoveWGPUTextureFromCache(
    const wgpu::Device& device,
    const wgpu::Texture& texture) {
  auto* texture_cache = GetWGPUTextureCache(device);
  if (!texture_cache) {
    return;
  }

  texture_cache->erase(texture.GetUsage());
}

void DawnSharedTextureHolder::MaybeCacheWGPUTexture(
    const wgpu::Device& device,
    const wgpu::Texture& texture) {
  if (!texture) {
    return;
  }

  auto* texture_cache = GetWGPUTextureCache(device);
  if (!texture_cache) {
    return;
  }

  // Determine whether `texture` needs to be cached.
  auto texture_usage = texture.GetUsage();
  auto [iter, _] = texture_cache->emplace(texture_usage, texture);
  CHECK_EQ(texture.Get(), iter->second.Get());
}

void DawnSharedTextureHolder::DestroyWGPUTextureIfNotCached(
    const wgpu::Device& device,
    const wgpu::Texture& texture) {
  if (auto cached_texture = GetCachedWGPUTexture(device, texture.GetUsage())) {
    CHECK_EQ(cached_texture.Get(), texture.Get());
    return;
  }

  texture.Destroy();
}

void DawnSharedTextureHolder::EraseDataIfDeviceLost() {
  // Clear out any cached SharedTextureMemory instances for which the
  // associated Device has been lost. This both saves memory and more
  // importantly ensures that a new SharedTextureMemory instance will be
  // created if another Device occupies the same memory as a previously-used,
  // now-lost Device.
  std::vector<WGPUDevice> devices_to_erase;
  for (auto& [wgpu_device, shared_texture_data] : shared_texture_data_cache_) {
    if (!shared_texture_data.memory.IsDeviceLost()) {
      continue;
    }
    devices_to_erase.push_back(wgpu_device);
  }

  for (const auto& wgpu_device : devices_to_erase) {
    shared_texture_data_cache_.erase(wgpu_device);
  }
}

}  // namespace gpu
