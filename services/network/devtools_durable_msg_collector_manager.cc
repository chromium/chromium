// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/devtools_durable_msg_collector_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/features.h"

namespace network {

namespace {
// The name of the dump provider.
constexpr const char* kDumpProviderName =
    "DevtoolsDurableMessageCollectorManager";
}  // namespace

DevtoolsDurableMessageCollectorManager::DevtoolsDurableMessageCollectorManager()
    : max_global_buffer_size_(static_cast<uint64_t>(
          network::features::kDurableMessagesGlobalBufferSize.Get())) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, kDumpProviderName,
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

DevtoolsDurableMessageCollectorManager::
    ~DevtoolsDurableMessageCollectorManager() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void DevtoolsDurableMessageCollectorManager::AddCollector(
    mojo::PendingReceiver<network::mojom::DurableMessageCollector> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<DevtoolsDurableMessageCollector>(
                                  weak_factory_.GetWeakPtr()),
                              std::move(receiver));
}

void DevtoolsDurableMessageCollectorManager::OnCollectorCreated(
    DevtoolsDurableMessageCollector* collector) {
  collectors_.insert(collector);
}

void DevtoolsDurableMessageCollectorManager::OnCollectorDestroyed(
    DevtoolsDurableMessageCollector* collector) {
  // An O(n) scan is fine here, because the number of collectors is
  // expected to be small.
  for (auto it = profile_collectors_.begin();
       it != profile_collectors_.end();) {
    if (it->second == collector) {
      it = profile_collectors_.erase(it);
    } else {
      ++it;
    }
  }
  collectors_.erase(collector);
}

bool DevtoolsDurableMessageCollectorManager::CanAccommodate(size_t size) const {
  if (max_global_buffer_size_ == 0) {
    // No global limit set, so we can accommodate any size.
    return true;
  }
  if (total_memory_usage_ >= max_global_buffer_size_) {
    return false;
  }
  return size <= max_global_buffer_size_ - total_memory_usage_;
}

void DevtoolsDurableMessageCollectorManager::OnCollectorAddedBytes(
    size_t size) {
  total_memory_usage_ += size;
}

void DevtoolsDurableMessageCollectorManager::OnCollectorRemovedBytes(
    size_t size) {
  DCHECK_GE(total_memory_usage_, size);
  total_memory_usage_ -= size;
}

void DevtoolsDurableMessageCollectorManager::OnCollectorAddedMessage(
    size_t count) {
  total_message_count_ += count;
}

void DevtoolsDurableMessageCollectorManager::OnCollectorRemovedMessage(
    size_t count) {
  DCHECK_GE(total_message_count_, count);
  total_message_count_ -= count;
}

bool DevtoolsDurableMessageCollectorManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump("devtools/durable_message_collectors");
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  total_memory_usage_);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  total_message_count_);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  collectors_.size());
  return true;
}

std::vector<DevtoolsDurableMessageCollector*>
DevtoolsDurableMessageCollectorManager::GetCollectorsEnabledForProfile(
    const base::UnguessableToken& profile_id) {
  std::vector<DevtoolsDurableMessageCollector*> collectors;
  auto range = profile_collectors_.equal_range(profile_id);
  for (auto it = range.first; it != range.second; ++it) {
    collectors.push_back(it->second);
  }
  return collectors;
}

void DevtoolsDurableMessageCollectorManager::EnableForProfile(
    const base::UnguessableToken& profile_id,
    DevtoolsDurableMessageCollector& collector) {
  profile_collectors_.insert({profile_id, &collector});
}

void DevtoolsDurableMessageCollectorManager::DisableForProfile(
    const base::UnguessableToken& profile_id,
    DevtoolsDurableMessageCollector& collector) {
  auto range = profile_collectors_.equal_range(profile_id);
  for (auto it = range.first; it != range.second;) {
    if (it->second == &collector) {
      it = profile_collectors_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace network
