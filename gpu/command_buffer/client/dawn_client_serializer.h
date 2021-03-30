// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_SERIALIZER_H_
#define GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_SERIALIZER_H_

#include <dawn_wire/WireClient.h>

#include <memory>

#include "gpu/command_buffer/client/transfer_buffer.h"

namespace gpu {

struct SharedMemoryLimits;
class TransferBuffer;

namespace webgpu {

class DawnClientMemoryTransferService;
class WebGPUCmdHelper;
class WebGPUImplementation;

class DawnClientSerializer final : public dawn_wire::CommandSerializer {
 public:
  static std::unique_ptr<DawnClientSerializer> Create(
      WebGPUImplementation* client,
      WebGPUCmdHelper* helper,
      DawnClientMemoryTransferService* memory_transfer_service,
      const SharedMemoryLimits& limits);

  DawnClientSerializer(WebGPUImplementation* client,
                       WebGPUCmdHelper* helper,
                       DawnClientMemoryTransferService* memory_transfer_service,
                       std::unique_ptr<TransferBuffer> transfer_buffer,
                       uint32_t buffer_initial_size);
  ~DawnClientSerializer() override;

  // dawn_wire::CommandSerializer implementation
  size_t GetMaximumAllocationSize() const final;
  void* GetCmdSpace(size_t size) final;
  bool Flush() final;

  // Signal that it's important that the previously encoded commands are
  // flushed. Calling |AwaitingFlush| will return whether or not a flush still
  // needs to occur.
  void SetAwaitingFlush(bool awaiting_flush);

  // Check if the serializer has commands that have been serialized but not
  // flushed after |SetAwaitingFlush| was passed |true|.
  bool AwaitingFlush() const { return awaiting_flush_; }

  // Disconnect the serializer. Commands are forgotten and future calls to
  // |GetCmdSpace| will do nothing.
  void Disconnect();

 private:
  WebGPUImplementation* client_;
  WebGPUCmdHelper* helper_;
  DawnClientMemoryTransferService* memory_transfer_service_;
  uint32_t put_offset_ = 0;
  std::unique_ptr<TransferBuffer> transfer_buffer_;
  uint32_t buffer_initial_size_;
  ScopedTransferBufferPtr buffer_;

  bool awaiting_flush_ = false;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_SERIALIZER_H_
