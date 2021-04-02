// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_SERVICE_MEMORY_TRANSFER_SERVICE_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_SERVICE_MEMORY_TRANSFER_SERVICE_H_

#include <dawn_wire/WireServer.h>

namespace gpu {

class CommonDecoder;

namespace webgpu {

class DawnServiceMemoryTransferService final
    : public dawn_wire::server::MemoryTransferService {
 public:
  DawnServiceMemoryTransferService(CommonDecoder* decoder);
  ~DawnServiceMemoryTransferService() override;

  // Deserialize data to create Read/Write handles. These handles are for the
  // client to Read/Write data. The serialized data is a MemoryTransferHandle
  // which contains the id, offset, and size of a shared memory region. If the
  // decoded region is invalid, these functions return false and result in a
  // context lost.
  // The Read and Write handles, respectively, are used to
  //  1) Copy from GPU service memory into client-visible shared memory.
  //  2) Copy from client-visible shared memory into GPU service memory.
  bool DeserializeReadHandle(const void* deserialize_pointer,
                             size_t deserialize_size,
                             ReadHandle** read_handle) override;

  bool DeserializeWriteHandle(const void* deserialize_pointer,
                              size_t deserialize_size,
                              WriteHandle** write_handle) override;

 private:
  CommonDecoder* decoder_;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_SERVICE_MEMORY_TRANSFER_SERVICE_H_
