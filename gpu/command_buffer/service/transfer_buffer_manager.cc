// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/transfer_buffer_manager.h"

#include <stdint.h>

#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/service/memory_tracking.h"

namespace gpu {

TransferBufferManager::TransferBufferManager(MemoryTracker* memory_tracker)
    : shared_memory_bytes_allocated_(0), memory_tracker_(memory_tracker) {
  // When created from InProcessCommandBuffer, we won't have a |memory_tracker_|
  // so don't register a dump provider.
  if (memory_tracker_) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "gpu::TransferBufferManager",
        base::ThreadTaskRunnerHandle::Get());
  }
}

TransferBufferManager::~TransferBufferManager() {
  while (!registered_buffers_.empty()) {
    BufferMap::iterator it = registered_buffers_.begin();
    DCHECK(shared_memory_bytes_allocated_ >= it->second->size());
    shared_memory_bytes_allocated_ -= it->second->size();
    registered_buffers_.erase(it);
  }
  DCHECK(!shared_memory_bytes_allocated_);

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool TransferBufferManager::RegisterTransferBuffer(
    int32_t id,
    scoped_refptr<Buffer> buffer) {
  if (id <= 0) {
    DVLOG(0) << "Cannot register transfer buffer with non-positive ID.";
    return false;
  }

  // Fail if the ID is in use.
  if (registered_buffers_.find(id) != registered_buffers_.end()) {
    DVLOG(0) << "Buffer ID already in use.";
    return false;
  }

  // Check buffer alignment is sane.
  DCHECK(!(reinterpret_cast<uintptr_t>(buffer->memory()) &
           (kCommandBufferEntrySize - 1)));

  shared_memory_bytes_allocated_ += buffer->size();

  registered_buffers_[id] = std::move(buffer);

  return true;
}

void TransferBufferManager::DestroyTransferBuffer(int32_t id) {
  BufferMap::iterator it = registered_buffers_.find(id);
  if (it == registered_buffers_.end()) {
    DVLOG(0) << "Transfer buffer ID was not registered.";
    return;
  }

  DCHECK(shared_memory_bytes_allocated_ >= it->second->size());
  shared_memory_bytes_allocated_ -= it->second->size();

  registered_buffers_.erase(it);
}

scoped_refptr<Buffer> TransferBufferManager::GetTransferBuffer(int32_t id) {
  if (id == 0)
    return nullptr;

  BufferMap::iterator it = registered_buffers_.find(id);
  if (it == registered_buffers_.end())
    return nullptr;

  return it->second;
}

bool TransferBufferManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryDumpLevelOfDetail;

  if (args.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND) {
    std::string dump_name = base::StringPrintf("gpu/transfer_memory/client_%d",
                                               memory_tracker_->ClientId());
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes,
                    shared_memory_bytes_allocated_);

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  for (const auto& buffer_entry : registered_buffers_) {
    int32_t buffer_id = buffer_entry.first;
    const Buffer* buffer = buffer_entry.second.get();
    std::string dump_name =
        base::StringPrintf("gpu/transfer_memory/client_%d/buffer_%d",
                           memory_tracker_->ClientId(), buffer_id);
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, buffer->size());

    auto shared_memory_guid = buffer->backing()->GetGUID();
    if (!shared_memory_guid.is_empty()) {
      pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), shared_memory_guid,
                                           0 /* importance */);
    } else {
      auto guid = GetBufferGUIDForTracing(memory_tracker_->ClientTracingId(),
                                          buffer_id);
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid);
    }
  }

  return true;
}

}  // namespace gpu
