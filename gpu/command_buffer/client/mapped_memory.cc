// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/mapped_memory.h"

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/buffer.h"

namespace gpu {
namespace {

// Generates process-unique IDs to use for tracing a MappedMemoryManager's
// chunks.
base::AtomicSequenceNumber g_next_mapped_memory_manager_tracing_id;

}  // namespace

MemoryChunk::MemoryChunk(int32_t shm_id,
                         scoped_refptr<gpu::Buffer> shm,
                         CommandBufferHelper* helper)
    : shm_id_(shm_id),
      shm_(shm),
      allocator_(shm->size(), helper, shm->as_byte_span()) {}

MemoryChunk::~MemoryChunk() = default;

MappedMemoryManager::MappedMemoryManager(CommandBufferHelper* helper,
                                         size_t unused_memory_reclaim_limit)
    : chunk_size_multiple_(FencedAllocator::kAllocAlignment),
      helper_(helper),
      allocated_memory_(0),
      max_free_bytes_(unused_memory_reclaim_limit),
      max_allocated_bytes_(SharedMemoryLimits::kNoLimit),
      dedicated_memory_for_testing_(0),
      max_dedicated_bytes_(SharedMemoryLimits::kNoLimit),
      tracing_id_(g_next_mapped_memory_manager_tracing_id.GetNext()) {}

MappedMemoryManager::~MappedMemoryManager() {
  // Outstanding ScopedDedicatedChunk instances hold a raw pointer back to
  // this manager; destroying it first would leave them dangling.
  DCHECK(dedicated_chunks_.empty());
  helper_->OrderingBarrier();
  CommandBuffer* cmd_buf = helper_->command_buffer();
  for (auto& chunk : chunks_) {
    cmd_buf->DestroyTransferBuffer(chunk->shm_id());
  }
}

base::span<uint8_t> MappedMemoryManager::Alloc(
    unsigned int size,
    int32_t* shm_id,
    unsigned int* shm_offset,
    TransferBufferAllocationOption option) {
  DCHECK(shm_id);
  DCHECK(shm_offset);
  if (size <= allocated_memory_) {
    size_t total_bytes_in_use = 0;
    // See if any of the chunks can satisfy this request.
    for (auto& chunk : chunks_) {
      chunk->FreeUnused();
      total_bytes_in_use += chunk->bytes_in_use();
      if (chunk->GetLargestFreeSizeWithoutWaiting() >= size) {
        auto span = chunk->Alloc(size);
        DCHECK(!span.empty());
        *shm_id = chunk->shm_id();
        *shm_offset = chunk->GetOffset(span.data());
        return span;
      }
    }

    // If there is a memory limit being enforced and total free
    // memory (allocated_memory_ - total_bytes_in_use) is larger than
    // the limit try waiting.
    if (max_free_bytes_ != SharedMemoryLimits::kNoLimit &&
        (allocated_memory_ - total_bytes_in_use) >= max_free_bytes_) {
      TRACE_EVENT0("gpu", "MappedMemoryManager::Alloc::wait");
      for (auto& chunk : chunks_) {
        if (chunk->GetLargestFreeSizeWithWaiting() >= size) {
          auto span = chunk->Alloc(size);
          DCHECK(!span.empty());
          *shm_id = chunk->shm_id();
          *shm_offset = chunk->GetOffset(span.data());
          return span;
        }
      }
    }
  }

  if (max_allocated_bytes_ != SharedMemoryLimits::kNoLimit &&
      (allocated_memory_ + size) > max_allocated_bytes_) {
    return {};
  }

  // Make a new chunk to satisfy the request.
  CommandBuffer* cmd_buf = helper_->command_buffer();
  base::CheckedNumeric<uint32_t> chunk_size = size;
  chunk_size = (size + chunk_size_multiple_ - 1) & ~(chunk_size_multiple_ - 1);
  uint32_t safe_chunk_size = 0;
  if (!chunk_size.AssignIfValid(&safe_chunk_size)) {
    return {};
  }

  int32_t id = -1;
  scoped_refptr<gpu::Buffer> shm = cmd_buf->CreateTransferBuffer(
      safe_chunk_size, &id, /* alignment */ 0, option);
  if (id < 0) {
    return {};
  }
  DCHECK(shm.get());
  MemoryChunk* mc = new MemoryChunk(id, shm, helper_);
  allocated_memory_ += mc->GetSize();
  chunks_.push_back(base::WrapUnique(mc));
  auto span = mc->Alloc(size);
  DCHECK(!span.empty());
  *shm_id = mc->shm_id();
  *shm_offset = mc->GetOffset(span.data());
  return span;
}

ScopedDedicatedChunk MappedMemoryManager::AllocDedicatedChunk(uint32_t size) {
  if (max_allocated_bytes_ != SharedMemoryLimits::kNoLimit &&
      (size > max_allocated_bytes_ - allocated_memory_)) {
    return {};
  }

  if (max_dedicated_bytes_ != SharedMemoryLimits::kNoLimit &&
      (size > max_dedicated_bytes_ - dedicated_memory_for_testing_)) {
    return {};
  }

  // Make a new chunk to guarantee offset 0.
  CommandBuffer* cmd_buf = helper_->command_buffer();
  int32_t id = -1;
  scoped_refptr<gpu::Buffer> shm = cmd_buf->CreateTransferBuffer(
      size, &id, /* alignment */ 0,
      TransferBufferAllocationOption::kReturnNullOnOOM);
  if (id < 0) {
    return {};
  }
  DCHECK(shm.get());
  base::span<uint8_t> span = shm->as_byte_span();
  allocated_memory_ += shm->size();
  dedicated_memory_for_testing_ += shm->size();
  dedicated_chunks_.emplace(id, std::move(shm));
  return ScopedDedicatedChunk(span.first(size), id, this);
}

void MappedMemoryManager::Free(void* pointer) {
  for (auto& chunk : chunks_) {
    if (chunk->IsInChunk(pointer)) {
      chunk->Free(pointer);
      return;
    }
  }
  NOTREACHED();
}

void MappedMemoryManager::RemoveDedicatedChunk(int32_t shm_id) {
  auto iter = dedicated_chunks_.find(shm_id);
  CHECK(iter != dedicated_chunks_.end());
  // DestroyTransferBuffer is sent out-of-band, so it would otherwise reach the
  // service before the commands referencing `shm_id` that are still sitting in
  // the command buffer.
  helper_->OrderingBarrier();
  helper_->command_buffer()->DestroyTransferBuffer(shm_id);
  allocated_memory_ -= iter->second->size();
  dedicated_memory_for_testing_ -= iter->second->size();
  dedicated_chunks_.erase(iter);
}

void MappedMemoryManager::FreePendingToken(void* pointer, int32_t token) {
  for (auto& chunk : chunks_) {
    if (chunk->IsInChunk(pointer)) {
      chunk->FreePendingToken(pointer, token);
      return;
    }
  }
  NOTREACHED();
}

void MappedMemoryManager::FreeUnused() {
  CommandBuffer* cmd_buf = helper_->command_buffer();
  MemoryChunkVector::iterator iter = chunks_.begin();
  while (iter != chunks_.end()) {
    MemoryChunk* chunk = (*iter).get();
    chunk->FreeUnused();
    if (chunk->bytes_in_use() == 0u) {
      if (chunk->InUseOrFreePending())
        helper_->OrderingBarrier();
      cmd_buf->DestroyTransferBuffer(chunk->shm_id());
      allocated_memory_ -= chunk->GetSize();
      iter = chunks_.erase(iter);
    } else {
      ++iter;
    }
  }
}

bool MappedMemoryManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryDumpLevelOfDetail;

  if (args.level_of_detail == MemoryDumpLevelOfDetail::kBackground) {
    std::string dump_name =
        base::StringPrintf("gpu/mapped_memory/manager_0x%x", tracing_id_);
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, allocated_memory_);

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  const uint64_t tracing_process_id =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->GetTracingProcessId();
  auto dump_chunk = [&](int32_t shm_id, uint32_t size, uint32_t free_size,
                        gpu::Buffer* shared_memory) {
    std::string dump_name = base::StringPrintf(
        "gpu/mapped_memory/manager_0x%x/chunk_0x%x", tracing_id_, shm_id);
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);

    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, size);
    dump->AddScalar("free_size", MemoryAllocatorDump::kUnitsBytes, free_size);

    auto shared_memory_guid = shared_memory->backing()->GetGUID();
    const int kImportance = 2;
    if (!shared_memory_guid.is_empty()) {
      pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), shared_memory_guid,
                                           kImportance);
    } else {
      auto guid = GetBufferGUIDForTracing(tracing_process_id, shm_id);
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  };
  for (const auto& chunk : chunks_) {
    dump_chunk(chunk->shm_id(), chunk->GetSize(), chunk->GetFreeSize(),
               chunk->shared_memory());
  }
  for (const auto& entry : dedicated_chunks_) {
    dump_chunk(entry.first, entry.second->size(), /*free_size=*/0u,
               entry.second.get());
  }

  return true;
}

FencedAllocator::State MappedMemoryManager::GetPointerStatusForTest(
    void* pointer,
    int32_t* token_if_pending) {
  for (auto& chunk : chunks_) {
    if (chunk->IsInChunk(pointer)) {
      return chunk->GetPointerStatusForTest(pointer, token_if_pending);
    }
  }
  return FencedAllocator::FREE;
}

ScopedDedicatedChunk::ScopedDedicatedChunk() = default;

ScopedDedicatedChunk::ScopedDedicatedChunk(base::span<uint8_t> span,
                                           int32_t shm_id,
                                           MappedMemoryManager* manager)
    : span_(span), shm_id_(shm_id), manager_(manager) {}

ScopedDedicatedChunk::ScopedDedicatedChunk(ScopedDedicatedChunk&& other)
    : span_(std::exchange(other.span_, base::raw_span<uint8_t>())),
      shm_id_(std::exchange(other.shm_id_, -1)),
      manager_(std::exchange(other.manager_, nullptr)) {}

ScopedDedicatedChunk& ScopedDedicatedChunk::operator=(
    ScopedDedicatedChunk&& other) {
  if (this != &other) {
    Reset();
    std::swap(span_, other.span_);
    std::swap(shm_id_, other.shm_id_);
    std::swap(manager_, other.manager_);
  }
  return *this;
}

ScopedDedicatedChunk::~ScopedDedicatedChunk() {
  Reset();
}

void ScopedDedicatedChunk::Reset() {
  if (manager_) {
    span_ = {};
    manager_->RemoveDedicatedChunk(shm_id_);
    shm_id_ = -1;
    manager_ = nullptr;
  }
}

void ScopedMappedMemoryPtr::Release() {
  if (valid()) {
    mapped_memory_manager_->FreePendingToken(buffer_.data(),
                                             helper_->InsertToken());
    buffer_ = {};
    shm_id_ = 0;
    shm_offset_ = 0;

    if (flush_after_release_) {
      helper_->CommandBufferHelper::Flush();
    }
  }
}

void ScopedMappedMemoryPtr::Reset(uint32_t new_size) {
  Release();

  if (new_size) {
    buffer_ = mapped_memory_manager_->Alloc(new_size, &shm_id_, &shm_offset_);
  }
}

}  // namespace gpu
