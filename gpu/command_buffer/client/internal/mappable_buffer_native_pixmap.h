// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_NATIVE_PIXMAP_H_
#define GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_NATIVE_PIXMAP_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/gpu_command_buffer_client_export.h"
#include "gpu/command_buffer/client/internal/mappable_buffer.h"

namespace gfx {
class ClientNativePixmap;
class ClientNativePixmapFactory;
}  // namespace gfx

namespace gpu {

class ClientSharedImage;

// Implementation of MappableBuffer based on Ozone native pixmap.
class GPU_COMMAND_BUFFER_CLIENT_EXPORT MappableBufferNativePixmap
    : public MappableBuffer {
 public:
  MappableBufferNativePixmap(const MappableBufferNativePixmap&) = delete;
  MappableBufferNativePixmap& operator=(const MappableBufferNativePixmap&) =
      delete;

  ~MappableBufferNativePixmap() override;

  static constexpr gfx::GpuMemoryBufferType kBufferType = gfx::NATIVE_PIXMAP;

  static std::unique_ptr<MappableBufferNativePixmap> CreateFromHandleForTesting(
      gfx::ClientNativePixmapFactory* client_native_pixmap_factory,
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage) {
    return CreateFromHandle(client_native_pixmap_factory, std::move(handle),
                            size, format, usage);
  }

  static base::OnceClosure AllocateForTesting(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage,
      gfx::GpuMemoryBufferHandle* handle);

  // Overridden from MappableBuffer:
  bool Map() override;
  void MapAsync(base::OnceCallback<void(bool)> callback) override;
  bool AsyncMappingIsNonBlocking() const override;
  void* memory(size_t plane) override;
  void Unmap() override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferType GetType() const override;
  gfx::GpuMemoryBufferHandle CloneHandle() const override;

 private:
  friend class ClientSharedImage;

  static std::unique_ptr<MappableBufferNativePixmap> CreateFromHandle(
      gfx::ClientNativePixmapFactory* client_native_pixmap_factory,
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage);

  MappableBufferNativePixmap(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      std::unique_ptr<gfx::ClientNativePixmap> native_pixmap);

  void AssertMapped();

  const gfx::Size size_;
  const viz::SharedImageFormat format_;
  const std::unique_ptr<gfx::ClientNativePixmap> pixmap_;

  // Note: This lock must be held throughout the entirety of the Map() and
  // Unmap() operations to avoid corrupt mutation across multiple threads.
  base::Lock map_lock_;
  uint32_t map_count_ GUARDED_BY(map_lock_) = 0u;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_NATIVE_PIXMAP_H_
