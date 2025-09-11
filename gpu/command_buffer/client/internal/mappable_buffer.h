// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_H_
#define GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_H_

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gpu_command_buffer_client_export.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace gpu {

// Abstraction for a CPU-mappable buffer that is used internally by
// ClientSharedImage to implement mappable SharedImages.
class GPU_COMMAND_BUFFER_CLIENT_EXPORT MappableBuffer {
 public:
  using CopyNativeBufferToShMemCallback =
      base::RepeatingCallback<void(gfx::GpuMemoryBufferHandle,
                                   base::UnsafeSharedMemoryRegion,
                                   base::OnceCallback<void(bool)>)>;

  MappableBuffer(const MappableBuffer&) = delete;
  MappableBuffer& operator=(const MappableBuffer&) = delete;

  virtual ~MappableBuffer() = default;

  // Maps each plane of the buffer into the client's address space so it can be
  // written to by the CPU. This call may block, for instance if the GPU needs
  // to finish accessing the buffer or if CPU caches need to be synchronized.
  // Returns false on failure.
  virtual bool Map() = 0;

  // Maps each plane of the buffer into the client's address space so it can be
  // written to by the CPU. By default, implementations are blocking and just
  // call Map(). However, on some platforms the implementations are
  // non-blocking. In that case the result callback will be executed on the
  // GpuMemoryThread if some work in the GPU service is required for mapping, or
  // will be executed immediately in the current sequence. Warning: Make sure
  // the GMB isn't destroyed before the callback is run otherwise GPU process
  // might try to write in destroyed shared memory region. Don't attempt to
  // Unmap or get memory before the callback is executed. Otherwise a CHECK will
  // fire.
  virtual void MapAsync(base::OnceCallback<void(bool)> result_cb) = 0;

  // Indicates if the `MapAsync` is non-blocking. Otherwise it's just calling
  // `Map()` directly.
  virtual bool AsyncMappingIsNonBlocking() const = 0;

  // Returns a pointer to the memory address of a plane. Buffer must have been
  // successfully mapped using a call to Map() before calling this function.
  virtual void* memory(size_t plane) = 0;

  // Unmaps the buffer. It's illegal to use any pointer returned by memory()
  // after this has been called.
  virtual void Unmap() = 0;

  // Fills the stride in bytes for each plane of the buffer. The stride of
  // plane K is stored at index K-1 of the |stride| array.
  virtual int stride(size_t plane) const = 0;

  // Returns the type of this buffer.
  virtual gfx::GpuMemoryBufferType GetType() const = 0;

  // Returns a platform specific handle for this buffer which in particular can
  // be sent over IPC. This duplicates file handles as appropriate, so that a
  // caller takes ownership of the returned handle.
  virtual gfx::GpuMemoryBufferHandle CloneHandle() const = 0;

#if BUILDFLAG(IS_WIN)
  // Used to set the use_premapped_memory flag in the MappableBufferDXGI to
  // indicate whether to use the premapped memory or not. It is only used with
  // MappableSI. See MappableBufferDXGI override for more details.
  virtual void SetUsePreMappedMemory(bool use_premapped_memory) = 0;
#endif

 protected:
  MappableBuffer() = default;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_H_
