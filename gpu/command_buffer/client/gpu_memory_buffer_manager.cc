// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "ui/gfx/buffer_format_util.h"

namespace gpu {

GpuMemoryBufferManager::GpuMemoryBufferManager() = default;

GpuMemoryBufferManager::~GpuMemoryBufferManager() = default;

GpuMemoryBufferManager::AllocatedBufferInfo::AllocatedBufferInfo(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format)
    : buffer_id_(handle.id),
      type_(handle.type),
      size_in_bytes_(gfx::BufferSizeForBufferFormat(size, format)) {
  DCHECK_NE(gfx::EMPTY_BUFFER, type_);

  if (type_ == gfx::SHARED_MEMORY_BUFFER)
    shared_memory_guid_ = handle.region.GetGUID();
}

GpuMemoryBufferManager::AllocatedBufferInfo::~AllocatedBufferInfo() = default;

bool GpuMemoryBufferManager::AllocatedBufferInfo::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    int client_id,
    uint64_t client_tracing_process_id) const {
  base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
      base::StringPrintf("gpumemorybuffer/client_0x%" PRIX32 "/buffer_%d",
                         client_id, buffer_id_.id));
  if (!dump)
    return false;

  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  size_in_bytes_);

  // Create the shared ownership edge to avoid double counting memory.
  if (type_ == gfx::SHARED_MEMORY_BUFFER) {
    pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), shared_memory_guid_,
                                         /*importance=*/0);
  } else {
    auto shared_buffer_guid = gfx::GetGenericSharedGpuMemoryGUIDForTracing(
        client_tracing_process_id, buffer_id_);
    pmd->CreateSharedGlobalAllocatorDump(shared_buffer_guid);
    pmd->AddOwnershipEdge(dump->guid(), shared_buffer_guid);
  }

  return true;
}

}  // namespace gpu
