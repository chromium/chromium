// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"

#include <vector>

namespace memory_instrumentation {

GlobalMemoryDump::GlobalMemoryDump(
    std::vector<mojom::ProcessMemoryDumpPtr> process_dumps,
    mojom::AggregatedMetricsPtr aggregated_metrics)
    : aggregated_metrics_(std::move(aggregated_metrics)) {
  auto it = process_dumps_.before_begin();
  for (mojom::ProcessMemoryDumpPtr& process_dump : process_dumps) {
    it = process_dumps_.emplace_after(it, std::move(process_dump));
  }
}
GlobalMemoryDump::~GlobalMemoryDump() = default;

std::unique_ptr<GlobalMemoryDump> GlobalMemoryDump::MoveFrom(
    mojom::GlobalMemoryDumpPtr ptr) {
  return ptr ? std::unique_ptr<GlobalMemoryDump>(
                   new GlobalMemoryDump(std::move(ptr->process_dumps),
                                        std::move(ptr->aggregated_metrics)))
             : nullptr;
}

GlobalMemoryDump::ProcessDump::ProcessDump(
    mojom::ProcessMemoryDumpPtr process_dump)
    : raw_dump_(std::move(process_dump)) {}
GlobalMemoryDump::ProcessDump::~ProcessDump() = default;

std::optional<uint64_t> GlobalMemoryDump::ProcessDump::GetMetric(
    const std::string& dump_name,
    const std::string& metric_name) const {
  auto dump_it = raw_dump_->chrome_allocator_dumps.find(dump_name);
  if (dump_it == raw_dump_->chrome_allocator_dumps.cend())
    return std::nullopt;

  auto metric_it = dump_it->second->numeric_entries.find(metric_name);
  if (metric_it == dump_it->second->numeric_entries.cend())
    return std::nullopt;

  return std::optional<uint64_t>(metric_it->second);
}

GlobalMemoryDump::AggregatedMetrics::AggregatedMetrics(
    mojom::AggregatedMetricsPtr aggregated_metrics)
    : aggregated_metrics_(aggregated_metrics.is_null()
                              ? mojom::AggregatedMetrics::New()
                              : std::move(aggregated_metrics)) {}

GlobalMemoryDump::AggregatedMetrics::~AggregatedMetrics() = default;

}  // namespace memory_instrumentation
