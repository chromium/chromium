// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/command_buffer_direct_locked.h"

namespace gpu {

void CommandBufferDirectLocked::Flush(int32_t put_offset) {
  flush_count_++;
  client_put_offset_ = put_offset;
  if (!flush_locked_)
    DoFlush();
}

CommandBuffer::State CommandBufferDirectLocked::WaitForTokenInRange(
    int32_t start,
    int32_t end) {
  State state = GetLastState();
  if (state.error != error::kNoError || InRange(start, end, state.token)) {
    return state;
  } else {
    DoFlush();
    return CommandBufferDirect::WaitForTokenInRange(start, end);
  }
}

CommandBuffer::State CommandBufferDirectLocked::WaitForGetOffsetInRange(
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end) {
  State state = GetLastState();
  if (state.error != error::kNoError ||
      (InRange(start, end, state.get_offset) &&
       (set_get_buffer_count == state.set_get_buffer_count))) {
    return state;
  } else {
    DoFlush();
    return CommandBufferDirect::WaitForGetOffsetInRange(set_get_buffer_count,
                                                        start, end);
  }
}

scoped_refptr<Buffer> CommandBufferDirectLocked::CreateTransferBuffer(
    uint32_t size,
    int32_t* id,
    uint32_t alignment,
    TransferBufferAllocationOption option) {
  if (fail_create_transfer_buffer_) {
    *id = -1;
    return nullptr;
  } else {
    return CommandBufferDirect::CreateTransferBuffer(size, id, alignment,
                                                     option);
  }
}

}  // namespace gpu
