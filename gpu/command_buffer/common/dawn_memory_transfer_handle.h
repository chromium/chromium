// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_DAWN_MEMORY_TRANSFER_HANDLE_H_
#define GPU_COMMAND_BUFFER_COMMON_DAWN_MEMORY_TRANSFER_HANDLE_H_

namespace gpu {
namespace webgpu {

// Describes the type of the transfer buffer. `Shared` means the transfer buffer
// is allocated on the shared memory that can be shared among multiple transfer
// buffers. `Dedicated` means the transfer buffer has its own dedicated memory
// allocation.
enum class TransferBufferType : uint32_t {
  kShared = 0u,
  kDedicated = 1u,
};

// This struct holds information describing a shared memory allocation used for
// bulk data transfers between the Dawn client and service. The shared memory is
// allocated by the client using MappedMemoryManager.
// On the GPU service, shared memory is received using
// CommonDecoder::GetSharedMemoryAs which checks the memory region is valid.
struct MemoryTransferHandle {
  uint32_t size;
  int32_t shm_id;
  uint32_t shm_offset;
  TransferBufferType type;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_DAWN_MEMORY_TRANSFER_HANDLE_H_
