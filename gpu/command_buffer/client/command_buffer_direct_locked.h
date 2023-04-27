// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_COMMAND_BUFFER_DIRECT_LOCKED_H_
#define GPU_COMMAND_BUFFER_CLIENT_COMMAND_BUFFER_DIRECT_LOCKED_H_

#include "gpu/command_buffer/service/command_buffer_direct.h"

namespace gpu {

// A CommandBuffer that allows "locking" flushes, that is delaying progress
// until either it gets unlocked or the client waits for progress.
class CommandBufferDirectLocked : public CommandBufferDirect {
 public:
  CommandBufferDirectLocked() = default;

  CommandBufferDirectLocked(const CommandBufferDirectLocked&) = delete;
  CommandBufferDirectLocked& operator=(const CommandBufferDirectLocked&) =
      delete;

  ~CommandBufferDirectLocked() override = default;

  // Overridden from CommandBufferDirect
  void Flush(int32_t put_offset) override;
  CommandBuffer::State WaitForTokenInRange(int32_t start, int32_t end) override;
  CommandBuffer::State WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                               int32_t start,
                                               int32_t end) override;
  scoped_refptr<Buffer> CreateTransferBuffer(
      uint32_t size,
      int32_t* id,
      uint32_t alignment = 0,
      TransferBufferAllocationOption option =
          TransferBufferAllocationOption::kLoseContextOnOOM) override;

  void LockFlush() { flush_locked_ = true; }

  void UnlockFlush() { flush_locked_ = false; }

  int FlushCount() { return flush_count_; }

  int GetServicePutOffset() { return service_put_offset_; }

  void set_fail_create_transfer_buffer(bool fail) {
    fail_create_transfer_buffer_ = fail;
  }

 private:
  void DoFlush() {
    CommandBufferDirect::Flush(client_put_offset_);
    service_put_offset_ = client_put_offset_;
  }

  bool fail_create_transfer_buffer_ = false;
  bool flush_locked_ = false;
  int client_put_offset_ = 0;
  int service_put_offset_ = 0;
  int flush_count_ = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_COMMAND_BUFFER_DIRECT_LOCKED_H_
