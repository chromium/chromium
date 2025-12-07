// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_shared_texture_cache.h"

#include "base/check_op.h"

namespace gpu {

DawnSharedTextureCache::TextureMetadata::TextureMetadata(
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage,
    const std::vector<wgpu::TextureFormat>& view_formats)
    : usage(usage),
      internal_usage(internal_usage),
      view_formats(view_formats) {}

DawnSharedTextureCache::TextureMetadata::~TextureMetadata() = default;

DawnSharedTextureCache::TextureMetadata::TextureMetadata(TextureMetadata&&) =
    default;
DawnSharedTextureCache::TextureMetadata&
DawnSharedTextureCache::TextureMetadata::operator=(TextureMetadata&&) = default;

DawnSharedTextureCache::SharedTextureData::SharedTextureData() = default;

DawnSharedTextureCache::SharedTextureData::~SharedTextureData() {
  for (auto& [_, texture] : texture_cache) {
    texture.Destroy();
  }
}

DawnSharedTextureCache::SharedTextureData::SharedTextureData(
    SharedTextureData&&) = default;
DawnSharedTextureCache::SharedTextureData&
DawnSharedTextureCache::SharedTextureData::operator=(SharedTextureData&&) =
    default;

DawnSharedTextureCache::DawnSharedTextureCache() = default;

DawnSharedTextureCache::~DawnSharedTextureCache() = default;

DawnSharedTextureCache::TextureCache* DawnSharedTextureCache::GetTextureCache(
    const wgpu::Device& device) {
  auto iter = shared_texture_data_cache_.find(device.Get());
  if (iter == shared_texture_data_cache_.end()) {
    return nullptr;
  }
  return &iter->second.texture_cache;
}

wgpu::SharedTextureMemory DawnSharedTextureCache::GetSharedTextureMemory(
    const wgpu::Device& device) {
  auto iter = shared_texture_data_cache_.find(device.Get());
  if (iter == shared_texture_data_cache_.end()) {
    return nullptr;
  }
  return iter->second.shared_texture_memory;
}

void DawnSharedTextureCache::MaybeCacheSharedTextureMemory(
    const wgpu::Device& device,
    const wgpu::SharedTextureMemory& shared_texture_memory) {
  if (!shared_texture_memory) {
    return;
  }

  // Return early if the STM is already cached.
  if (auto cached = GetSharedTextureMemory(device)) {
    CHECK_EQ(cached.Get(), shared_texture_memory.Get());
    return;
  }

  SharedTextureData shared_texture_data;
  shared_texture_data.shared_texture_memory = shared_texture_memory;
  shared_texture_data_cache_.emplace(device.Get(),
                                     std::move(shared_texture_data));
}

wgpu::Texture DawnSharedTextureCache::GetCachedWGPUTexture(
    const wgpu::Device& device,
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage,
    const std::vector<wgpu::TextureFormat>& view_formats) {
  auto* texture_cache = GetTextureCache(device);
  if (!texture_cache) {
    return nullptr;
  }

  auto iter = texture_cache->find({usage, internal_usage, view_formats});
  if (iter == texture_cache->end()) {
    return nullptr;
  }

  return iter->second;
}

void DawnSharedTextureCache::RemoveWGPUTextureFromCache(
    const wgpu::Device& device,
    const wgpu::Texture& texture) {
  auto* texture_cache = GetTextureCache(device);
  if (!texture_cache) {
    return;
  }

  base::EraseIf(*texture_cache, [&](const auto& kv_pair) {
    return kv_pair.second.Get() == texture.Get();
  });
}

void DawnSharedTextureCache::MaybeCacheWGPUTexture(
    const wgpu::Device& device,
    const wgpu::Texture& texture,
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage,
    const std::vector<wgpu::TextureFormat>& view_formats) {
  if (!texture) {
    return;
  }

  auto* texture_cache = GetTextureCache(device);
  if (!texture_cache) {
    return;
  }

  auto [iter, _] = texture_cache->try_emplace(
      TextureMetadata{usage, internal_usage, view_formats}, texture);
  CHECK_EQ(texture.Get(), iter->second.Get());
}

void DawnSharedTextureCache::DestroyWGPUTextureIfNotCached(
    const wgpu::Device& device,
    const wgpu::Texture& texture) {
  if (!texture) {
    return;
  }

  auto* texture_cache = GetTextureCache(device);
  if (texture_cache) {
    auto iter = std::ranges::find_if(*texture_cache, [&](const auto& kv_pair) {
      return kv_pair.second.Get() == texture.Get();
    });
    if (iter != texture_cache->end()) {
      return;
    }
  }

  texture.Destroy();
}

void DawnSharedTextureCache::EraseDataIfDeviceLost() {
  // Clear out any cached SharedTextureMemory instances for which the
  // associated Device has been lost. This both saves memory and more
  // importantly ensures that a new SharedTextureMemory instance will be
  // created if another Device occupies the same memory as a previously-used,
  // now-lost Device.
  std::vector<WGPUDevice> devices_to_erase;
  for (auto& [wgpu_device, shared_texture_data] : shared_texture_data_cache_) {
    if (!shared_texture_data.shared_texture_memory.IsDeviceLost()) {
      continue;
    }
    devices_to_erase.push_back(wgpu_device);
  }

  for (const auto& wgpu_device : devices_to_erase) {
    shared_texture_data_cache_.erase(wgpu_device);
  }
}

}  // namespace gpu
