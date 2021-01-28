// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_SERIALIZER_H_
#define GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_SERIALIZER_H_

#include <dawn_wire/WireClient.h>

#include <memory>

#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/client/webgpu_interface.h"

namespace gpu {

class TransferBuffer;

namespace webgpu {

class DawnClientMemoryTransferService;
class WebGPUCmdHelper;

class DawnClientSerializer final : public dawn_wire::CommandSerializer {
 public:
  DawnClientSerializer(DawnDeviceClientID device_client_id,
                       WebGPUCmdHelper* helper,
                       DawnClientMemoryTransferService* memory_transfer_service,
                       std::unique_ptr<TransferBuffer> c2s_transfer_buffer);
  ~DawnClientSerializer() override;

  // Send WGPUDeviceProperties to the server side
  // Note that this function should only be called once for each
  // DawnClientSerializer object.
  void RequestDeviceCreation(
      uint32_t requested_adapter_id,
      const WGPUDeviceProperties& requested_device_properties);

  // dawn_wire::CommandSerializer implementation
  size_t GetMaximumAllocationSize() const final;
  void* GetCmdSpace(size_t size) final;
  bool Flush() final;

  void SetClientAwaitingFlush(bool awaiting_flush);
  bool ClientAwaitingFlush() const { return client_awaiting_flush_; }

  // Called upon context lost.
  void HandleGpuControlLostContext();

  // For the WebGPUInterface implementation of WebGPUImplementation
  WGPUDevice GetDevice() const;
  ReservedTexture ReserveTexture();
  bool HandleCommands(const char* commands, size_t command_size);

 private:
  DawnDeviceClientID device_client_id_;
  WebGPUCmdHelper* helper_;
  DawnClientMemoryTransferService* memory_transfer_service_;

  std::unique_ptr<dawn_wire::WireClient> wire_client_;

  uint32_t c2s_buffer_default_size_ = 0;
  uint32_t c2s_put_offset_ = 0;
  std::unique_ptr<TransferBuffer> c2s_transfer_buffer_;
  ScopedTransferBufferPtr c2s_buffer_;

  bool client_awaiting_flush_ = false;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_DAWN_CLIENT_SERIALIZER_H_
