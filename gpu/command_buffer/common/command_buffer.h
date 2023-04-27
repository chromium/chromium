// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_
#define GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/gpu_export.h"

namespace gpu {

// Common interface for CommandBuffer implementations.
class GPU_EXPORT CommandBuffer {
 public:
  struct State {
    State()
        : get_offset(0),
          token(-1),
          release_count(0),
          error(error::kNoError),
          context_lost_reason(error::kUnknown),
          generation(0),
          set_get_buffer_count(0) {}

    // The offset (in entries) from which the reader is reading.
    int32_t get_offset;

    // The current token value. This is used by the writer to defer
    // changes to shared memory objects until the reader has reached a certain
    // point in the command buffer. The reader is responsible for updating the
    // token value, for example in response to an asynchronous set token command
    // embedded in the command buffer. The default token value is zero.
    int32_t token;

    // The fence sync release count. Incremented by InsertFenceSync commands.
    // Used by the client to monitor sync token progress.
    uint64_t release_count;

    // Error status.
    error::Error error;

    // Lost context detail information.
    error::ContextLostReason context_lost_reason;

    // Generation index of this state. The generation index is incremented every
    // time a new state is retrieved from the command processor, so that
    // consistency can be kept even if IPC messages are processed out-of-order.
    uint32_t generation;

    // Number of times SetGetBuffer was called. This allows the client to verify
    // that |get| corresponds (or not) to the last buffer it set.
    uint32_t set_get_buffer_count;
  };

  struct ConsoleMessage {
    // An user supplied id.
    int32_t id;
    // The message.
    std::string message;
  };

  CommandBuffer() = default;

  CommandBuffer(const CommandBuffer&) = delete;
  CommandBuffer& operator=(const CommandBuffer&) = delete;

  virtual ~CommandBuffer() = default;

  // Check if a value is between a start and end value, inclusive, allowing
  // for wrapping if start > end.
  static bool InRange(int32_t start, int32_t end, int32_t value) {
    if (start <= end)
      return start <= value && value <= end;
    else
      return start <= value || value <= end;
  }

  // Returns the last state without synchronizing with the service.
  virtual State GetLastState() = 0;

  // The writer calls this to update its put offset. This ensures the reader
  // sees the latest added commands, and will eventually process them. On the
  // service side, commands are processed up to the given put_offset before
  // subsequent Flushes on the same GpuChannel.
  virtual void Flush(int32_t put_offset) = 0;

  // As Flush, ensures that on the service side, commands up to put_offset
  // are processed but before subsequent commands on the same GpuChannel but
  // flushing to the service may be deferred.
  virtual void OrderingBarrier(int32_t put_offset) = 0;

  // The writer calls this to wait until the current token is within a
  // specific range, inclusive. Can return early if an error is generated.
  virtual State WaitForTokenInRange(int32_t start, int32_t end) = 0;

  // The writer calls this to wait until the current get offset is within a
  // specific range, inclusive, after SetGetBuffer was called exactly
  // set_get_buffer_count times. Can return early if an error is generated.
  virtual State WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                        int32_t start,
                                        int32_t end) = 0;

  // Sets the buffer commands are read from.
  // Also resets the get and put offsets to 0, and increments
  // set_get_buffer_count.
  virtual void SetGetBuffer(int32_t transfer_buffer_id) = 0;

  // Create a transfer buffer of the given size. Returns its ID or -1 on
  // error.
  // The |option| argument defaults to kLoseContextOnOOM, but may be
  // kReturnNullOnOOM. Passing kReturnNullOnOOM will gracefully fail and return
  // nullptr on OOM instead of losing the context. Callers should be careful
  // to check error conditions.
  virtual scoped_refptr<gpu::Buffer> CreateTransferBuffer(
      uint32_t size,
      int32_t* id,
      uint32_t alignment = 0,
      TransferBufferAllocationOption option =
          TransferBufferAllocationOption::kLoseContextOnOOM) = 0;

  // Destroy a transfer buffer. The ID must be positive.
  // An ordering barrier must be placed after any commands that use the buffer
  // before it is safe to call this function to destroy it.
  virtual void DestroyTransferBuffer(int32_t id) = 0;

  // Forcibly lose this context. Used by higher-level code when it determines
  // the necessity to do so. Has no effect if the context has already been lost.
  virtual void ForceLostContext(error::ContextLostReason reason) = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_
