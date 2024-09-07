// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the command buffer helper class.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CMD_BUFFER_HELPER_H_
#define GPU_COMMAND_BUFFER_CLIENT_CMD_BUFFER_HELPER_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/check_op.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/gpu_export.h"

namespace gpu {

class Buffer;

#if !BUILDFLAG(IS_ANDROID)
#define CMD_HELPER_PERIODIC_FLUSH_CHECK
const int kCommandsPerFlushCheck = 100;
const int kPeriodicFlushDelayInMicroseconds = 500;
#endif

const int kAutoFlushSmall = 16;  // 1/16 of the buffer
const int kAutoFlushBig = 2;     // 1/2 of the buffer

// Command buffer helper class. This class simplifies ring buffer management:
// it will allocate the buffer, give it to the buffer interface, and let the
// user add commands to it, while taking care of the synchronization (put and
// get). It also provides a way to ensure commands have been executed, through
// the token mechanism:
//
// helper.AddCommand(...);
// helper.AddCommand(...);
// int32_t token = helper.InsertToken();
// helper.AddCommand(...);
// helper.AddCommand(...);
// [...]
//
// helper.WaitForToken(token);  // this doesn't return until the first two
//                              // commands have been executed.
class GPU_EXPORT CommandBufferHelper {
 public:
  explicit CommandBufferHelper(CommandBuffer* command_buffer);

  CommandBufferHelper(const CommandBufferHelper&) = delete;
  CommandBufferHelper& operator=(const CommandBufferHelper&) = delete;

  virtual ~CommandBufferHelper();

  // Initializes the CommandBufferHelper.
  // Parameters:
  //   ring_buffer_size: The size of the ring buffer portion of the command
  //       buffer.
  gpu::ContextResult Initialize(uint32_t ring_buffer_size);

  // Sets whether the command buffer should automatically flush periodically
  // to try to increase performance. Defaults to true.
  void SetAutomaticFlushes(bool enabled);

  // True if the context is lost.
  bool IsContextLost();

  // Asynchronously flushes the commands, setting the put pointer to let the
  // buffer interface know that new commands have been added. After a flush
  // returns, the command buffer service is aware of all pending commands.
  void Flush();

  // Flushes if the put pointer has changed since the last flush.
  void FlushLazy();

  // Ensures that commands up to the put pointer will be processed in the
  // command buffer service before any future commands on other command buffers
  // sharing a channel.
  void OrderingBarrier();

  // Waits until all the commands have been executed. Returns whether it
  // was successful. The function will fail if the command buffer service has
  // disconnected.
  bool Finish();

  // Waits until a given number of available entries are available.
  // Parameters:
  //   count: number of entries needed. This value must be at most
  //     the size of the buffer minus one.
  void WaitForAvailableEntries(int32_t count);

  // Inserts a new token into the command buffer. This token either has a value
  // different from previously inserted tokens, or ensures that previously
  // inserted tokens with that value have already passed through the command
  // stream.
  // Returns:
  //   the value of the new token or -1 if the command buffer reader has
  //   shutdown.
  int32_t InsertToken();

  // Returns true if the token has passed.  This combines RefreshCachedToken
  // and HasCachedTokenPassed.  Don't call this function if you have to call
  // it repeatedly, and instead use those alternative functions.
  // Parameters:
  //   the value of the token to check whether it has passed
  bool HasTokenPassed(int32_t token);

  // Returns true if the token has passed, but doesn't take a lock and check
  // for what the latest token state is.
  bool HasCachedTokenPassed(int32_t token);

  // Update the state of the latest passed token.
  void RefreshCachedToken();

  // Waits until the token of a particular value has passed through the command
  // stream (i.e. commands inserted before that token have been executed).
  // NOTE: This will call Flush if it needs to block.
  // Parameters:
  //   the value of the token to wait for.
  void WaitForToken(int32_t token);

  // Called prior to each command being issued. Waits for a certain amount of
  // space to be available. Returns address of space.
  void* GetSpace(int32_t entries) {
#if defined(CMD_HELPER_PERIODIC_FLUSH_CHECK)
    // Allow this command buffer to be pre-empted by another if a "reasonable"
    // amount of work has been done. On highend machines, this reduces the
    // latency of GPU commands. However, on Android, this can cause the
    // kernel to thrash between generating GPU commands and executing them.
    ++commands_issued_;
    if (flush_automatically_ &&
        (commands_issued_ % kCommandsPerFlushCheck == 0)) {
      PeriodicFlushCheck();
    }
#endif

    // Test for immediate entries.
    if (entries > immediate_entry_count_) {
      WaitForAvailableEntries(entries);
      if (entries > immediate_entry_count_)
        return nullptr;
    }

    DCHECK_LE(entries, immediate_entry_count_);

    // Allocate space and advance put_.
    CommandBufferEntry* space = &entries_[put_];
    put_ += entries;
    immediate_entry_count_ -= entries;

    DCHECK_LE(put_, total_entry_count_);
    return space;
  }

  // Typed version of GetSpace. Gets enough room for the given type and returns
  // a reference to it.
  template <typename T>
  T* GetCmdSpace() {
    static_assert(T::kArgFlags == cmd::kFixed,
                  "T::kArgFlags should equal cmd::kFixed");
    int32_t space_needed = ComputeNumEntries(sizeof(T));
    T* data = static_cast<T*>(GetSpace(space_needed));
    return data;
  }

  // Typed version of GetSpace for immediate commands.
  template <typename T>
  T* GetImmediateCmdSpace(size_t data_space) {
    static_assert(T::kArgFlags == cmd::kAtLeastN,
                  "T::kArgFlags should equal cmd::kAtLeastN");
    int32_t space_needed = ComputeNumEntries(sizeof(T) + data_space);
    T* data = static_cast<T*>(GetSpace(space_needed));
    return data;
  }

  // Typed version of GetSpace for immediate commands.
  template <typename T>
  T* GetImmediateCmdSpaceTotalSize(size_t total_space) {
    static_assert(T::kArgFlags == cmd::kAtLeastN,
                  "T::kArgFlags should equal cmd::kAtLeastN");
    int32_t space_needed = ComputeNumEntries(total_space);
    T* data = static_cast<T*>(GetSpace(space_needed));
    return data;
  }

  // Common Commands
  void Noop(uint32_t skip_count) {
    cmd::Noop* cmd = GetImmediateCmdSpace<cmd::Noop>(
        (skip_count - 1) * sizeof(CommandBufferEntry));
    if (cmd) {
      cmd->Init(skip_count);
    }
  }

  void SetToken(uint32_t token) {
    cmd::SetToken* cmd = GetCmdSpace<cmd::SetToken>();
    if (cmd) {
      cmd->Init(token);
    }
  }

  void SetBucketSize(uint32_t bucket_id, uint32_t size) {
    cmd::SetBucketSize* cmd = GetCmdSpace<cmd::SetBucketSize>();
    if (cmd) {
      cmd->Init(bucket_id, size);
    }
  }

  void SetBucketData(uint32_t bucket_id,
                     uint32_t offset,
                     uint32_t size,
                     uint32_t shared_memory_id,
                     uint32_t shared_memory_offset) {
    cmd::SetBucketData* cmd = GetCmdSpace<cmd::SetBucketData>();
    if (cmd) {
      cmd->Init(bucket_id, offset, size, shared_memory_id,
                shared_memory_offset);
    }
  }

  void SetBucketDataImmediate(uint32_t bucket_id,
                              uint32_t offset,
                              const void* data,
                              uint32_t size) {
    cmd::SetBucketDataImmediate* cmd =
        GetImmediateCmdSpace<cmd::SetBucketDataImmediate>(size);
    if (cmd) {
      cmd->Init(bucket_id, offset, size);
      memcpy(ImmediateDataAddress(cmd), data, size);
    }
  }

  void GetBucketStart(uint32_t bucket_id,
                      uint32_t result_memory_id,
                      uint32_t result_memory_offset,
                      uint32_t data_memory_size,
                      uint32_t data_memory_id,
                      uint32_t data_memory_offset) {
    cmd::GetBucketStart* cmd = GetCmdSpace<cmd::GetBucketStart>();
    if (cmd) {
      cmd->Init(bucket_id, result_memory_id, result_memory_offset,
                data_memory_size, data_memory_id, data_memory_offset);
    }
  }

  void GetBucketData(uint32_t bucket_id,
                     uint32_t offset,
                     uint32_t size,
                     uint32_t shared_memory_id,
                     uint32_t shared_memory_offset) {
    cmd::GetBucketData* cmd = GetCmdSpace<cmd::GetBucketData>();
    if (cmd) {
      cmd->Init(bucket_id, offset, size, shared_memory_id,
                shared_memory_offset);
    }
  }

  uint64_t InsertFenceSync(base::FunctionRef<uint64_t()> sync_token_generator) {
    cmd::InsertFenceSync* cmd = GetCmdSpace<cmd::InsertFenceSync>();

    // Please note that it is important to generate the sync token after
    // GetCmdSpace().
    // 1) If InsertFenceSync command `cmd` is not successfully allocated, a sync
    //    token shouldn't be created either. Otherwise, it results in waiting
    //    for a fence sync that is never released.
    // 2) Even if `cmd` is successfully allocated, we still need to generate the
    //    sync token afterwards: The GetCmdSpace() call may result in a flush of
    //    the command buffer. On the other hand, command buffer implementations
    //    (such as CommandBufferProxyImpl) may assume that when a flush happens,
    //    the commands releasing the previously-generated sync tokens are
    //    already in the buffer and thus all flushed.
    if (cmd) {
      uint64_t release_count = sync_token_generator();
      cmd->Init(release_count);
      return release_count;
    }

    return 0;
  }

  CommandBuffer* command_buffer() const { return command_buffer_; }

  scoped_refptr<Buffer> get_ring_buffer() const { return ring_buffer_; }
  int32_t get_ring_buffer_id() const { return ring_buffer_id_; }

  uint32_t flush_generation() const { return flush_generation_; }

  void FreeRingBuffer();

  bool HaveRingBuffer() const {
    bool have_ring_buffer = !!ring_buffer_;
    DCHECK(usable() || !have_ring_buffer);
    return have_ring_buffer;
  }

  bool usable() const { return usable_; }

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd);

  int32_t GetPutOffsetForTest() const { return put_; }

 private:
  void CalcImmediateEntries(int waiting_count);
  bool AllocateRingBuffer();
  void SetGetBuffer(int32_t id, scoped_refptr<Buffer> buffer);

  // Waits for the get offset to be in a specific range, inclusive. Returns
  // false if there was an error.
  bool WaitForGetOffsetInRange(int32_t start, int32_t end);

#if defined(CMD_HELPER_PERIODIC_FLUSH_CHECK)
  // Calls Flush if automatic flush conditions are met.
  void PeriodicFlushCheck();
#endif

  int32_t GetTotalFreeEntriesNoWaiting() const;

  // Updates |cached_get_offset_|, |cached_last_token_read_| and |context_lost_|
  // from given command buffer state.
  void UpdateCachedState(const CommandBuffer::State& state);

  const raw_ptr<CommandBuffer> command_buffer_;
  int32_t ring_buffer_id_ = -1;
  uint32_t ring_buffer_size_ = 0;
  scoped_refptr<gpu::Buffer> ring_buffer_;
  raw_ptr<CommandBufferEntry, AllowPtrArithmetic> entries_ = nullptr;
  int32_t total_entry_count_ = 0;  // the total number of entries
  int32_t immediate_entry_count_ = 0;
  int32_t token_ = 0;
  int32_t put_ = 0;
  int32_t cached_last_token_read_ = 0;
  int32_t cached_get_offset_ = 0;
  uint32_t set_get_buffer_count_ = 0;
  bool service_on_old_buffer_ = false;

#if defined(CMD_HELPER_PERIODIC_FLUSH_CHECK)
  int commands_issued_ = 0;
#endif

  bool usable_ = true;
  bool context_lost_ = false;
  bool flush_automatically_ = true;

  // We track last put offset to avoid redundant automatic flushes. We track
  // both flush and ordering barrier put offsets so that an automatic flush
  // after an ordering barrier forces a flush. Automatic flushes are enabled on
  // desktop, and are also used to flush before waiting for free space in the
  // command buffer. If the auto flush logic is wrong, we might call
  // WaitForGetOffsetInRange without flushing, causing the service to go idle,
  // and the client to hang. See https://crbug.com/798400 for details.
  int32_t last_flush_put_ = 0;
  int32_t last_ordering_barrier_put_ = 0;

  base::TimeTicks last_flush_time_;

  // Incremented every time the helper flushes the command buffer.
  // Can be used to track when prior commands have been flushed.
  uint32_t flush_generation_ = 0;

  friend class CommandBufferHelperTest;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CMD_BUFFER_HELPER_H_
