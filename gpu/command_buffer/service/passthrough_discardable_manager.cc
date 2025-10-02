// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/passthrough_discardable_manager.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/config/gpu_preferences.h"

namespace gpu {

PassthroughDiscardableManager::DiscardableCacheValue::DiscardableCacheValue() =
    default;
PassthroughDiscardableManager::DiscardableCacheValue::DiscardableCacheValue(
    const DiscardableCacheValue& other) = default;
PassthroughDiscardableManager::DiscardableCacheValue::DiscardableCacheValue(
    DiscardableCacheValue&& other) = default;
PassthroughDiscardableManager::DiscardableCacheValue&
PassthroughDiscardableManager::DiscardableCacheValue::operator=(
    const DiscardableCacheValue& other) = default;
PassthroughDiscardableManager::DiscardableCacheValue&
PassthroughDiscardableManager::DiscardableCacheValue::operator=(
    DiscardableCacheValue&& other) = default;
PassthroughDiscardableManager::DiscardableCacheValue::~DiscardableCacheValue() =
    default;

PassthroughDiscardableManager::PassthroughDiscardableManager(
    const GpuPreferences& preferences)
    : cache_size_limit_(preferences.force_gpu_mem_discardable_limit_bytes
                            ? preferences.force_gpu_mem_discardable_limit_bytes
                            : DiscardableCacheSizeLimit()) {
  // In certain cases, SingleThreadTaskRunner::CurrentDefaultHandle isn't set
  // (Android Webview).  Don't register a dump provider in these cases.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "gpu::PassthroughDiscardableManager",
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

PassthroughDiscardableManager::~PassthroughDiscardableManager() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool PassthroughDiscardableManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryDumpLevelOfDetail;

  if (args.level_of_detail == MemoryDumpLevelOfDetail::kBackground) {
    std::string dump_name =
        base::StringPrintf("gpu/discardable_cache/cache_0x%" PRIXPTR,
                           reinterpret_cast<uintptr_t>(this));
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, total_size_);

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  return true;
}

void PassthroughDiscardableManager::DeleteContextGroup(
    const gles2::ContextGroup* context_group,
    bool has_context) {
  DCHECK(context_group);

  DeleteTextures(context_group, has_context);
}

void PassthroughDiscardableManager::OnContextLost() {
  DeleteTextures(nullptr, false);
}

void PassthroughDiscardableManager::DeleteTextures(
    const gles2::ContextGroup* context_group,
    bool has_context) {
}

void PassthroughDiscardableManager::DeleteTexture(
    uint32_t client_id,
    const gles2::ContextGroup* context_group) {
}

void PassthroughDiscardableManager::UpdateTextureSize(
    uint32_t client_id,
    const gles2::ContextGroup* context_group,
    size_t new_size) {
}

void PassthroughDiscardableManager::HandleMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  size_t limit = DiscardableCacheSizeLimitForPressure(cache_size_limit_,
                                                      memory_pressure_level);
  EnforceCacheSizeLimit(limit);
}

void PassthroughDiscardableManager::EnforceCacheSizeLimit(size_t limit) {
}

}  // namespace gpu
