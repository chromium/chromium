// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_handle_info.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

class GPU_EXPORT ClientSharedImage
    : public base::RefCountedThreadSafe<ClientSharedImage> {
 public:
  // Provides access to the CPU visible memory for the SharedImage if it is
  // being used for CPU READ/WRITE and underlying resource(native buffers/shared
  // memory) is CPU mappable. Memory and strides can be requested for each
  // plane.
  class GPU_EXPORT ScopedMapping {
   public:
    ~ScopedMapping();

    // Returns a pointer to the beginning of the plane.
    void* Memory(const uint32_t plane_index);

    // Returns plane stride.
    size_t Stride(const uint32_t plane_index);

    // Returns the size of the buffer.
    gfx::Size Size();

    // Returns BufferFormat.
    gfx::BufferFormat Format();

    // Returns whether the underlying resource is shared memory.
    bool IsSharedMemory();

    // Dumps information about the memory backing this instance to |pmd|.
    // The memory usage is attributed to |buffer_dump_guid|.
    // |tracing_process_id| uniquely identifies the process owning the memory.
    // |importance| is relevant only for the cases of co-ownership, the memory
    // gets attributed to the owner with the highest importance.
    void OnMemoryDump(
        base::trace_event::ProcessMemoryDump* pmd,
        const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
        uint64_t tracing_process_id,
        int importance);

   private:
    friend class ClientSharedImage;

    ScopedMapping();
    static std::unique_ptr<ScopedMapping> Create(
        gfx::GpuMemoryBuffer* gpu_memory_buffer);

    bool Init(gfx::GpuMemoryBuffer* gpu_memory_buffer);

    // ScopedMapping is essentially a wrapper around GpuMemoryBuffer for now for
    // simplicity and will be removed later.
    // TODO(crbug.com/1474697): Refactor/Rename GpuMemoryBuffer and its
    // implementations  as the end goal after all clients using GMB are
    // converted to use the ScopedMapping and notion of GpuMemoryBuffer is being
    // removed.
    raw_ptr<gfx::GpuMemoryBuffer> buffer_;
  };

  explicit ClientSharedImage(const Mailbox& mailbox);
  ClientSharedImage(const Mailbox& mailbox,
                    GpuMemoryBufferHandleInfo handle_info);

  const Mailbox& mailbox() { return mailbox_; }

  // Returns a clone of the GpuMemoryBufferHandle associated with this ClientSI.
  // Valid to call only if this instance was created with a non-null
  // GpuMemoryBuffer.
  gfx::GpuMemoryBufferHandle CloneGpuMemoryBufferHandle() const {
    CHECK(gpu_memory_buffer_);
    return gpu_memory_buffer_->CloneHandle();
  }

  base::trace_event::MemoryAllocatorDumpGuid GetGUIDForTracing() {
    return gpu::GetSharedImageGUIDForTracing(mailbox_);
  }

  // Maps |mailbox| into CPU visible memory and returns a ScopedMapping object
  // which can be used to read/write to the CPU mapped memory. The SharedImage
  // backing this ClientSI must have been created with CPU_READ/CPU_WRITE usage.
  std::unique_ptr<ScopedMapping> Map();

  static scoped_refptr<ClientSharedImage> CreateForTesting() {
    return base::MakeRefCounted<ClientSharedImage>(
        Mailbox::GenerateForSharedImage());
  }

 private:
  friend class base::RefCountedThreadSafe<ClientSharedImage>;
  ~ClientSharedImage();

  const Mailbox mailbox_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_
