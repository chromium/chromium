// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_SERIALIZER_H_
#define GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_SERIALIZER_H_

#include <dawn/wire/WireClient.h>

#include <memory>

#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/client/transfer_buffer.h"

namespace gpu {

class TransferBuffer;

namespace webgpu {

class DawnClientMemoryTransferService;
class WebGPUCmdHelper;
class WebGPUImplementation;

class DawnClientSerializer : public dawn::wire::CommandSerializer {
 public:
  DawnClientSerializer(WebGPUImplementation* client,
                       WebGPUCmdHelper* helper,
                       DawnClientMemoryTransferService* memory_transfer_service,
                       std::unique_ptr<TransferBuffer> transfer_buffer);
  ~DawnClientSerializer() override;

  // dawn::wire::CommandSerializer implementation
  size_t GetMaximumAllocationSize() const final;
  void* GetCmdSpace(size_t size) final;
#if DCHECK_IS_ON()
  void OnSerializeError() final;
#endif

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

  // Marks the commands' place in the GPU command buffer without flushing for
  // GPU execution.
  void Commit();

 private:
  // dawn::wire::CommandSerializer implementation
  bool Flush() final;

  // Found dangling on `linux-rel` in
  // `gpu_tests.context_lost_integration_test.ContextLostIntegrationTest.
  // ContextLost_WebGPUStressRequestDeviceAndRemoveLoop`
  raw_ptr<WebGPUImplementation, DanglingUntriaged> client_;
  raw_ptr<WebGPUCmdHelper, DanglingUntriaged> helper_;

  raw_ptr<DawnClientMemoryTransferService> memory_transfer_service_;
  uint32_t put_offset_ = 0;
  std::unique_ptr<TransferBuffer> transfer_buffer_;
  uint32_t buffer_initial_size_;
  ScopedTransferBufferPtr buffer_;

  bool awaiting_flush_ = false;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_SERIALIZER_H_
