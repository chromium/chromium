// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of the command buffer helper class.

#include "gpu/command_buffer/client/cmd_buffer_helper.h"

#include <stdint.h>

#include <algorithm>
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"

namespace gpu {

CommandBufferHelper::CommandBufferHelper(CommandBuffer* command_buffer)
    : command_buffer_(command_buffer) {}

void CommandBufferHelper::SetAutomaticFlushes(bool enabled) {
  flush_automatically_ = enabled;
  CalcImmediateEntries(0);
}

bool CommandBufferHelper::IsContextLost() {
  if (!context_lost_)
    context_lost_ = error::IsError(command_buffer()->GetLastState().error);
  return context_lost_;
}

void CommandBufferHelper::CalcImmediateEntries(int waiting_count) {
  DCHECK_GE(waiting_count, 0);

  // If not allocated, no entries are available. If not usable, it will not be
  // allocated.
  if (!HaveRingBuffer()) {
    immediate_entry_count_ = 0;
    return;
  }

  // Get maximum safe contiguous entries.
  const int32_t curr_get = cached_get_offset_;
  if (curr_get > put_) {
    immediate_entry_count_ = curr_get - put_ - 1;
  } else {
    immediate_entry_count_ =
        total_entry_count_ - put_ - (curr_get == 0 ? 1 : 0);
  }

  // Limit entry count to force early flushing.
  if (flush_automatically_) {
    int32_t limit =
        total_entry_count_ /
        ((curr_get == last_flush_put_) ? kAutoFlushSmall : kAutoFlushBig);

    int32_t pending = (put_ + total_entry_count_ - last_ordering_barrier_put_) %
                      total_entry_count_;

    if (pending > 0 && pending >= limit) {
      // Time to force flush.
      immediate_entry_count_ = 0;
    } else {
      // Limit remaining entries, but not lower than waiting_count entries to
      // prevent deadlock when command size is greater than the flush limit.
      limit -= pending;
      limit = limit < waiting_count ? waiting_count : limit;
      immediate_entry_count_ =
          immediate_entry_count_ > limit ? limit : immediate_entry_count_;
    }
  }
}

bool CommandBufferHelper::AllocateRingBuffer() {
  if (!usable()) {
    return false;
  }

  if (HaveRingBuffer()) {
    return true;
  }

  int32_t id = -1;
  scoped_refptr<Buffer> buffer =
      command_buffer_->CreateTransferBuffer(ring_buffer_size_, &id);
  if (id < 0) {
    usable_ = false;
    context_lost_ = true;
    CalcImmediateEntries(0);
    return false;
  }

  SetGetBuffer(id, std::move(buffer));
  return true;
}

void CommandBufferHelper::SetGetBuffer(int32_t id,
                                       scoped_refptr<Buffer> buffer) {
  command_buffer_->SetGetBuffer(id);
  entries_ = nullptr;
  total_entry_count_ = 0;
  ring_buffer_ = std::move(buffer);
  ring_buffer_id_ = id;
  ++set_get_buffer_count_;
  if (ring_buffer_) {
    entries_ = static_cast<CommandBufferEntry*>(ring_buffer_->memory());
    total_entry_count_ = ring_buffer_size_ / sizeof(CommandBufferEntry);
  }
  // Call to SetGetBuffer(id) above resets get and put offsets to 0.
  // No need to query it through IPC.
  put_ = 0;
  last_flush_put_ = 0;
  last_ordering_barrier_put_ = 0;
  cached_get_offset_ = 0;
  service_on_old_buffer_ = true;
  CalcImmediateEntries(0);
}

void CommandBufferHelper::FreeRingBuffer() {
  if (HaveRingBuffer()) {
    OrderingBarrier();
    command_buffer_->DestroyTransferBuffer(ring_buffer_id_);
    // SetGetBuffer is an IPC, so previous work needs to be flushed first.
    Flush();
    SetGetBuffer(-1, nullptr);
  }
}

gpu::ContextResult CommandBufferHelper::Initialize(uint32_t ring_buffer_size) {
  ring_buffer_size_ = ring_buffer_size;
  if (!AllocateRingBuffer()) {
    // This would fail if CreateTransferBuffer fails, which will not fail for
    // transient reasons such as context loss. See http://crrev.com/c/720269
    LOG(ERROR) << "ContextResult::kFatalFailure: "
               << "CommandBufferHelper::AllocateRingBuffer() failed";
    return gpu::ContextResult::kFatalFailure;
  }
  return gpu::ContextResult::kSuccess;
}

CommandBufferHelper::~CommandBufferHelper() {
  FreeRingBuffer();
}

void CommandBufferHelper::UpdateCachedState(const CommandBuffer::State& state) {
  // If the service hasn't seen the current get buffer yet (i.e. hasn't
  // processed the latest SetGetBuffer), it's as if it hadn't processed anything
  // in it, i.e. get == 0.
  service_on_old_buffer_ =
      (state.set_get_buffer_count != set_get_buffer_count_);
  cached_get_offset_ = service_on_old_buffer_ ? 0 : state.get_offset;
  cached_last_token_read_ = state.token;
  // Don't transition from a lost context to a working context.
  context_lost_ |= error::IsError(state.error);
}

bool CommandBufferHelper::WaitForGetOffsetInRange(int32_t start, int32_t end) {
  DCHECK(start >= 0 && start <= total_entry_count_);
  DCHECK(end >= 0 && end <= total_entry_count_);
  CommandBuffer::State last_state = command_buffer_->WaitForGetOffsetInRange(
      set_get_buffer_count_, start, end);
  UpdateCachedState(last_state);
  return !context_lost_;
}

void CommandBufferHelper::Flush() {
  TRACE_EVENT0("gpu", "CommandBufferHelper::Flush");
  // Wrap put_ before flush.
  if (put_ == total_entry_count_)
    put_ = 0;

  if (HaveRingBuffer()) {
    last_flush_time_ = base::TimeTicks::Now();
    last_flush_put_ = put_;
    last_ordering_barrier_put_ = put_;
    command_buffer_->Flush(put_);
    ++flush_generation_;
    CalcImmediateEntries(0);
  }
}

void CommandBufferHelper::FlushLazy() {
  if (put_ == last_flush_put_ && put_ == last_ordering_barrier_put_)
    return;
  Flush();
}

void CommandBufferHelper::OrderingBarrier() {
  // Wrap put_ before setting the barrier.
  if (put_ == total_entry_count_)
    put_ = 0;

  if (HaveRingBuffer()) {
    last_ordering_barrier_put_ = put_;
    command_buffer_->OrderingBarrier(put_);
    ++flush_generation_;
    CalcImmediateEntries(0);
  }
}

#if defined(CMD_HELPER_PERIODIC_FLUSH_CHECK)
void CommandBufferHelper::PeriodicFlushCheck() {
  base::TimeTicks current_time = base::TimeTicks::Now();
  if (current_time - last_flush_time_ >
      base::Microseconds(kPeriodicFlushDelayInMicroseconds)) {
    Flush();
  }
}
#endif

// Calls Flush() and then waits until the buffer is empty. Break early if the
// error is set.
bool CommandBufferHelper::Finish() {
  TRACE_EVENT0("gpu", "CommandBufferHelper::Finish");
  // If there is no work just exit.
  if (put_ == cached_get_offset_ && !service_on_old_buffer_) {
    return !context_lost_;
  }
  FlushLazy();
  if (!WaitForGetOffsetInRange(put_, put_))
    return false;
  DCHECK_EQ(cached_get_offset_, put_);

  CalcImmediateEntries(0);

  return true;
}

// Inserts a new token into the command stream. It uses an increasing value
// scheme so that we don't lose tokens (a token has passed if the current token
// value is higher than that token). Calls Finish() if the token value wraps,
// which will be rare. If we can't allocate a command buffer, token doesn't
// increase, ensuring WaitForToken eventually returns.
int32_t CommandBufferHelper::InsertToken() {
  // Increment token as 31-bit integer. Negative values are used to signal an
  // error.
  cmd::SetToken* cmd = GetCmdSpace<cmd::SetToken>();
  if (cmd) {
    token_ = (token_ + 1) & 0x7FFFFFFF;
    cmd->Init(token_);
    if (token_ == 0) {
      TRACE_EVENT0("gpu", "CommandBufferHelper::InsertToken(wrapped)");
      bool finished = Finish();  // we wrapped
      DCHECK(!finished || (cached_last_token_read_ == 0));
    }
  }
  return token_;
}

bool CommandBufferHelper::HasTokenPassed(int32_t token) {
  // If token_ wrapped around we Finish'd.
  if (token > token_)
    return true;
  // Don't update state if we don't have to.
  if (token <= cached_last_token_read_)
    return true;
  RefreshCachedToken();
  return token <= cached_last_token_read_;
}

void CommandBufferHelper::RefreshCachedToken() {
  CommandBuffer::State last_state = command_buffer_->GetLastState();
  UpdateCachedState(last_state);
}

bool CommandBufferHelper::HasCachedTokenPassed(int32_t token) {
  if (token > token_)
    return true;
  return token <= cached_last_token_read_;
}

// Waits until the current token value is greater or equal to the value passed
// in argument.
void CommandBufferHelper::WaitForToken(int32_t token) {
  DCHECK_GE(token, 0);
  if (HasTokenPassed(token))
    return;
  FlushLazy();
  CommandBuffer::State last_state =
      command_buffer_->WaitForTokenInRange(token, token_);
  UpdateCachedState(last_state);
}

// Waits for available entries, basically waiting until get >= put + count + 1.
// It actually waits for contiguous entries, so it may need to wrap the buffer
// around, adding a noops. Thus this function may change the value of put_. The
// function will return early if an error occurs, in which case the available
// space may not be available.
void CommandBufferHelper::WaitForAvailableEntries(int32_t count) {
  if (!AllocateRingBuffer())
    return;
  DCHECK(HaveRingBuffer());
  if (put_ + count > total_entry_count_) {
    // There's not enough room between the current put and the end of the
    // buffer, so we need to wrap. We will add noops all the way to the end,
    // but we need to make sure get wraps first, actually that get is 1 or
    // more (since put will wrap to 0 after we add the noops).
    DCHECK_LE(1, put_);
    int32_t curr_get = cached_get_offset_;
    if (curr_get > put_ || curr_get == 0) {
      TRACE_EVENT0("gpu", "CommandBufferHelper::WaitForAvailableEntries");
      FlushLazy();
      if (!WaitForGetOffsetInRange(1, put_))
        return;
      curr_get = cached_get_offset_;
      DCHECK_LE(curr_get, put_);
      DCHECK_NE(0, curr_get);
    }
    // Insert Noops to fill out the buffer.
    int32_t num_entries = total_entry_count_ - put_;
    while (num_entries > 0) {
      int32_t num_to_skip = std::min(CommandHeader::kMaxSize, num_entries);
      cmd::Noop::Set(&entries_[put_], num_to_skip);
      put_ += num_to_skip;
      num_entries -= num_to_skip;
    }
    put_ = 0;
  }

  // Try to get 'count' entries without flushing.
  CalcImmediateEntries(count);
  if (immediate_entry_count_ < count) {
    // Update cached_get_offset_ and try again.
    UpdateCachedState(command_buffer_->GetLastState());
    CalcImmediateEntries(count);
  }

  if (immediate_entry_count_ < count) {
    // Try again with a shallow Flush(). Flush can change immediate_entry_count_
    // because of the auto flush logic.
    FlushLazy();
    CalcImmediateEntries(count);
    if (immediate_entry_count_ < count) {
      // Buffer is full.  Need to wait for entries.
      TRACE_EVENT0("gpu", "CommandBufferHelper::WaitForAvailableEntries1");
      if (!WaitForGetOffsetInRange((put_ + count + 1) % total_entry_count_,
                                   put_))
        return;
      CalcImmediateEntries(count);
      if (immediate_entry_count_ < count) {
        // Tell the underlying command buffer to signal a lost context to higher
        // levels.
        command_buffer_->ForceLostContext(error::kGuilty);
        // Free the ring buffer and lose context.
        FreeRingBuffer();
        usable_ = false;
        context_lost_ = true;
        return;
      }
    }
  }
}

int32_t CommandBufferHelper::GetTotalFreeEntriesNoWaiting() const {
  int32_t current_get_offset = cached_get_offset_;
  if (current_get_offset > put_) {
    return current_get_offset - put_ - 1;
  } else {
    return current_get_offset + total_entry_count_ - put_ -
           (current_get_offset == 0 ? 1 : 0);
  }
}

bool CommandBufferHelper::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryDumpLevelOfDetail;

  if (!HaveRingBuffer())
    return true;

  const uint64_t tracing_process_id =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->GetTracingProcessId();

  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(base::StringPrintf(
      "gpu/command_buffer_memory/buffer_0x%x", ring_buffer_id_));
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, ring_buffer_size_);

  if (args.level_of_detail != MemoryDumpLevelOfDetail::kBackground) {
    dump->AddScalar(
        "free_size", MemoryAllocatorDump::kUnitsBytes,
        GetTotalFreeEntriesNoWaiting() * sizeof(CommandBufferEntry));
    base::UnguessableToken shared_memory_guid =
        ring_buffer_->backing()->GetGUID();
    const int kImportance = 2;
    if (!shared_memory_guid.is_empty()) {
      pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), shared_memory_guid,
                                           kImportance);
    } else {
      auto guid = GetBufferGUIDForTracing(tracing_process_id, ring_buffer_id_);
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  }
  return true;
}

}  // namespace gpu
