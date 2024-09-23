// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_
#define GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/service/async_api_interface.h"

namespace gpu {

class MemoryTracker;
class TransferBufferManager;

class GPU_EXPORT CommandBufferServiceBase {
 public:
  virtual ~CommandBufferServiceBase() = default;

  // Gets the current state of the service.
  virtual CommandBuffer::State GetState() = 0;

  // Set the release count for the last fence sync seen in the command stream.
  virtual void SetReleaseCount(uint64_t release_count) = 0;

  // Get the transfer buffer associated with an ID. Returns a null buffer for
  // ID 0.
  virtual scoped_refptr<gpu::Buffer> GetTransferBuffer(int32_t id) = 0;

  // Allows the reader to update the current token value.
  virtual void SetToken(int32_t token) = 0;

  // Allows the reader to set the current parse error.
  virtual void SetParseError(error::Error) = 0;

  // Allows the reader to set the current context lost reason.
  // NOTE: if calling this in conjunction with SetParseError,
  // call this first.
  virtual void SetContextLostReason(error::ContextLostReason) = 0;
};

class GPU_EXPORT CommandBufferServiceClient {
 public:
  enum CommandBatchProcessedResult {
    kContinueExecution,
    kPauseExecution,
  };

  virtual ~CommandBufferServiceClient() = default;

  // Called every time a batch of commands was processed by the
  // CommandBufferService. The return value indicates whether the
  // CommandBufferService should continue execution of commands or pause it.
  virtual CommandBatchProcessedResult OnCommandBatchProcessed() = 0;

  // Called when the CommandBufferService gets into an error state.
  virtual void OnParseError() = 0;
};

union CommandBufferEntry;

// An object that implements a shared memory command buffer and a synchronous
// API to manage the put and get pointers.
class GPU_EXPORT CommandBufferService : public CommandBufferServiceBase {
 public:
  static const int kParseCommandsSliceSmall = 20;
  static const int kParseCommandsSliceLarge = 100;

  CommandBufferService(CommandBufferServiceClient* client,
                       MemoryTracker* memory_tracker);

  CommandBufferService(const CommandBufferService&) = delete;
  CommandBufferService& operator=(const CommandBufferService&) = delete;

  ~CommandBufferService() override;

  // CommandBufferServiceBase implementation:
  CommandBuffer::State GetState() override;
  void SetReleaseCount(uint64_t release_count) override;
  scoped_refptr<Buffer> GetTransferBuffer(int32_t id) override;
  void SetToken(int32_t token) override;
  void SetParseError(error::Error error) override;
  void SetContextLostReason(error::ContextLostReason) override;

  // Setup the shared memory that shared state should be copied into.
  void SetSharedStateBuffer(std::unique_ptr<BufferBacking> shared_state_buffer);

  // Increments the generation and copies the current state into the shared
  // state transfer buffer.
  void UpdateState();

  // Flushes up to the put_offset and execute commands with the handler.
  void Flush(int32_t put_offset, AsyncAPIInterface* handler);

  // Sets the get buffer and call the GetBufferChangeCallback.
  void SetGetBuffer(int32_t transfer_buffer_id);

  // Registers an existing Buffer object with a given ID that can be used to
  // identify it in the command buffer.
  bool RegisterTransferBuffer(int32_t id, scoped_refptr<Buffer> buffer);

  // Unregisters and destroys the transfer buffer associated with the given id.
  void DestroyTransferBuffer(int32_t id);

  // Creates an in-process transfer buffer and register it with a newly created
  // id.
  scoped_refptr<Buffer> CreateTransferBuffer(uint32_t size,
                                             int32_t* id,
                                             uint32_t alignment = 0);

  // Creates an in-process transfer buffer and register it with a given id.
  scoped_refptr<Buffer> CreateTransferBufferWithId(uint32_t size,
                                                   int32_t id,
                                                   uint32_t alignment = 0);

  // Sets whether commands should be processed by this scheduler. Setting to
  // false unschedules. Setting to true reschedules.
  void SetScheduled(bool scheduled);

  bool scheduled() const { return scheduled_; }

  int32_t put_offset() const { return put_offset_; }

  void SetGetOffsetForTest(int32_t get_offset) {
    state_.get_offset = get_offset;
  }

  size_t GetSharedMemoryBytesAllocated() const;

  bool ShouldYield();

 private:
  raw_ptr<CommandBufferServiceClient> client_;
  std::unique_ptr<TransferBufferManager> transfer_buffer_manager_;

  CommandBuffer::State state_;
  int32_t put_offset_ = 0;

  int32_t num_entries_ = 0;
  scoped_refptr<Buffer> ring_buffer_;
  raw_ptr<volatile CommandBufferEntry, AllowPtrArithmetic> buffer_ = nullptr;

  std::unique_ptr<BufferBacking> shared_state_buffer_;
  raw_ptr<CommandBufferSharedState> shared_state_ = nullptr;

  // Whether the scheduler is currently able to process more commands.
  bool scheduled_ = true;
  bool paused_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_
