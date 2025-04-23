// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_SHARED_TEXTURE_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_SHARED_TEXTURE_CACHE_H_

#include <dawn/webgpu_cpp.h>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"

namespace gpu {

class DawnSharedTextureCache
    : public base::RefCountedThreadSafe<DawnSharedTextureCache> {
 public:
  DawnSharedTextureCache();

  // Returns a SharedTextureMemory for this device, or nullptr if there is no
  // instance.
  wgpu::SharedTextureMemory GetSharedTextureMemory(const wgpu::Device& device);
  // Inserts the SharedTextureMemory for this device, if not already present.
  void MaybeCacheSharedTextureMemory(
      const wgpu::Device& device,
      const wgpu::SharedTextureMemory& shared_texture_memory);

  // Returns the cached Texture for this device and texture_usage.
  wgpu::Texture GetCachedWGPUTexture(
      const wgpu::Device& device,
      wgpu::TextureUsage usage,
      wgpu::TextureUsage internal_usage,
      const std::vector<wgpu::TextureFormat>& view_formats);
  // Cache the `texture` for given device if it is not already cached.
  void MaybeCacheWGPUTexture(
      const wgpu::Device& device,
      const wgpu::Texture& texture,
      wgpu::TextureUsage usage,
      wgpu::TextureUsage internal_usage,
      const std::vector<wgpu::TextureFormat>& view_formats);
  // Remove the `texture` for given device from cache.
  void RemoveWGPUTextureFromCache(const wgpu::Device& device,
                                  const wgpu::Texture& texture);
  // Destroys the `texture` if it is not cached for the given device.
  void DestroyWGPUTextureIfNotCached(const wgpu::Device& device,
                                     const wgpu::Texture& texture);
  // Clear out all SharedTextureMemory instances for which the device has been
  // lost.
  void EraseDataIfDeviceLost();

 private:
  friend class base::RefCountedThreadSafe<DawnSharedTextureCache>;

  struct TextureMetadata {
    wgpu::TextureUsage usage;
    wgpu::TextureUsage internal_usage;
    std::vector<wgpu::TextureFormat> view_formats;

    TextureMetadata(wgpu::TextureUsage usage,
                    wgpu::TextureUsage internal_usage,
                    const std::vector<wgpu::TextureFormat>& view_formats);
    ~TextureMetadata();
    TextureMetadata(TextureMetadata&&);
    TextureMetadata& operator=(TextureMetadata&&);

    auto operator<=>(const TextureMetadata&) const = default;
  };

  using TextureCache = base::flat_map<TextureMetadata, wgpu::Texture>;

  struct SharedTextureData {
    SharedTextureData();
    ~SharedTextureData();
    SharedTextureData(SharedTextureData&&);
    SharedTextureData& operator=(SharedTextureData&&);

    wgpu::SharedTextureMemory shared_texture_memory;
    TextureCache texture_cache;
  };

  ~DawnSharedTextureCache();

  // Returns a pointer to the WGPUTextureCache instance for this device, or
  // nullptr if there is no instance.
  TextureCache* GetTextureCache(const wgpu::Device& device);

  // Per-Device SharedTextureData instances used to vend WebGPU textures for
  // the underlying native texture. The cache is keyed by raw pointers to the
  // Device as there is currently no better option. To ensure that we don't
  // incorrectly use a SharedTextureMemory instance for a lost Device that then
  // gets aliased by a newly-created Device, we drop all SharedTextureMemory
  // instances whose corresponding Device has been lost at the beginning of each
  // ProduceDawn() call before this cache is indexed by the passed-in Device.
  // TODO(crbug.com/40936879): Dawn should expose a unique ID per-Device, which
  // this cache should use as keys rather than raw pointers.
  base::flat_map<WGPUDevice, SharedTextureData> shared_texture_data_cache_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_SHARED_TEXTURE_CACHE_H_
