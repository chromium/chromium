// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_FALLBACK_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_FALLBACK_IMAGE_REPRESENTATION_H_

#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace gpu {
// Wraps a |SharedImageBacking| and exposes it as a wgpu::Texture by performing
// CPU readbacks/uploads.
// Note: the backing must implement UploadFromMemory & ReadbackToMemory.
class GPU_GLES2_EXPORT DawnFallbackImageRepresentation
    : public DawnImageRepresentation {
 public:
  DawnFallbackImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      wgpu::Device device,
      wgpu::TextureFormat wgpu_format,
      std::vector<wgpu::TextureFormat> view_formats);

  ~DawnFallbackImageRepresentation() override;

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage) final;
  void EndAccess() final;

 private:
  struct StagingBuffer {
    wgpu::Buffer buffer;
    gfx::Size plane_size;
    uint32_t bytes_per_row;
  };

  bool ComputeStagingBufferParams(int plane_index,
                                  uint32_t* bytes_per_row,
                                  size_t* bytes_per_plane) const;
  bool AllocateStagingBuffers(wgpu::BufferUsage usage,
                              bool map_at_creation,
                              std::vector<StagingBuffer>* buffers);
  SkPixmap MappedStagingBufferToPixmap(const StagingBuffer& staging_buffer,
                                       int plane_index,
                                       bool writable);

  bool ReadbackFromBacking();
  bool UploadToBacking();

  wgpu::Device device_;
  const wgpu::TextureFormat wgpu_format_;
  const std::vector<wgpu::TextureFormat> view_formats_;
  wgpu::Texture texture_;
};
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_FALLBACK_IMAGE_REPRESENTATION_H_
