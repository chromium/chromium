// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_MEMORY_TRANSFER_SERVICE_H_
#define GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_MEMORY_TRANSFER_SERVICE_H_

#include <dawn_wire/WireClient.h>
#include <vector>

namespace gpu {

class MappedMemoryManager;

namespace webgpu {

struct MemoryTransferHandle;

class DawnClientMemoryTransferService final
    : public dawn_wire::client::MemoryTransferService {
 public:
  DawnClientMemoryTransferService(MappedMemoryManager* mapped_memory);
  ~DawnClientMemoryTransferService() override;

  // Create a handle for reading shared memory data.
  // This may fail and return nullptr.
  ReadHandle* CreateReadHandle(size_t size) override;

  // Create a handle for writing shared memory data.
  // This may fail and return nullptr.
  WriteHandle* CreateWriteHandle(size_t size) override;

  // Free shared memory allocations after the token passes on the GPU process.
  void FreeHandlesPendingToken(int32_t token);

 private:
  class ReadHandleImpl;
  class WriteHandleImpl;

  // Allocate a shared memory handle for the memory transfer.
  void* AllocateHandle(size_t size, MemoryTransferHandle* handle);

  // Mark a shared memory allocation as free. This should not be called more
  // than once per block.
  void MarkHandleFree(void* ptr);

  MappedMemoryManager* mapped_memory_;
  // Pointers to memory allocated by the MappedMemoryManager to free after
  // the next Flush.
  std::vector<void*> free_blocks_;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_MEMORY_TRANSFER_SERVICE_H_
