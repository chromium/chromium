// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_MEMORY_TRANSFER_SERVICE_H_
#define GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_MEMORY_TRANSFER_SERVICE_H_

#include <dawn/wire/WireClient.h>

#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/client/mapped_memory.h"

namespace gpu {

class CommandBufferHelper;
class MappedMemoryManager;

namespace webgpu {

struct MemoryTransferHandle;

class DawnClientMemoryTransferService
    : public dawn::wire::client::MemoryTransferService {
 public:
  DawnClientMemoryTransferService(MappedMemoryManager* mapped_memory,
                                  bool enable_dedicated_transfer_buffer);
  ~DawnClientMemoryTransferService() override;

  // Create a handle for using shared memory data.
  // This may fail and return nullptr.
  std::unique_ptr<MemoryHandle> CreateMemoryHandle(
      size_t size,
      MemoryHandleUse memory_handle_use) override;

  // Free shared memory allocations after the next token passes on the GPU
  // process.
  void FreeHandles(CommandBufferHelper* helper);

  void Disconnect();

 private:
  class ReadHandleImpl;
  class WriteHandleImpl;
  class MemoryHandleImpl;
  class MemoryHandleImplWithDedicatedChunk;

  // Allocate a shared memory transfer buffer and populate `handle` with its
  // metadata (shm_id, shm_offset, size).
  base::span<std::byte> AllocateSharedTransferBuffer(
      size_t size,
      MemoryTransferHandle* handle);

  // Allocate a whole chunk dedicated to this allocation. Returns an invalid
  // chunk on failure.
  ScopedDedicatedChunk AllocateDedicatedChunk(size_t size);

  // Mark a shared memory allocation as free. This should not be called more
  // than once per block.
  void MarkHandleFree(void* ptr);

  // Take ownership of a dedicated chunk whose handle is gone, so that its
  // transfer buffer is destroyed on the next FreeHandles().
  void MarkDedicatedChunkFree(ScopedDedicatedChunk chunk);

  // Found dangling on `linux-rel` in
  // `gpu_tests.context_lost_integration_test.ContextLostIntegrationTest.
  // ContextLost_WebGPUStressRequestDeviceAndRemoveLoop`
  raw_ptr<MappedMemoryManager, DanglingUntriaged> mapped_memory_;

  // Pointers to memory allocated by the MappedMemoryManager to free after
  // the next Flush.
  std::vector<raw_ptr<void, VectorExperimental>> free_blocks_;

  // Dedicated chunks to destroy after the next Flush.
  std::vector<ScopedDedicatedChunk> free_dedicated_chunks_;

  // If disconnected, new handle creation always returns null.
  bool disconnected_ = false;

  // If false, never allocate didicated transfer buffers.
  bool enable_dedicated_transfer_buffer_ = false;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_MEMORY_TRANSFER_SERVICE_H_
