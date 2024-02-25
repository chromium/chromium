// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_GLOBAL_MEMORY_DUMP_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_GLOBAL_MEMORY_DUMP_H_

#include <forward_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

// The returned data structure to consumers of the memory_instrumentation
// service containing dumps for each process.
class COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION)
    GlobalMemoryDump {
 public:
  class COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION)
      ProcessDump {
   public:
    ProcessDump(mojom::ProcessMemoryDumpPtr process_memory_dump);

    ProcessDump(const ProcessDump&) = delete;
    ProcessDump& operator=(const ProcessDump&) = delete;

    ~ProcessDump();

    // Returns the metric for the given dump name and metric name. For example,
    // GetMetric("blink", "size") would return the aggregated sze of the
    // "blink/" dump.
    std::optional<uint64_t> GetMetric(const std::string& dump_name,
                                      const std::string& metric_name) const;

    base::ProcessId pid() const { return raw_dump_->pid; }
    mojom::ProcessType process_type() const { return raw_dump_->process_type; }
    const std::optional<std::string>& service_name() const {
      return raw_dump_->service_name;
    }

    const mojom::OSMemDump& os_dump() const { return *raw_dump_->os_dump; }

   private:
    mojom::ProcessMemoryDumpPtr raw_dump_;
  };

 public:
  class COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION)
      AggregatedMetrics {
   public:
    explicit AggregatedMetrics(mojom::AggregatedMetricsPtr aggregated_metrics);

    AggregatedMetrics(const AggregatedMetrics&) = delete;
    AggregatedMetrics& operator=(const AggregatedMetrics&) = delete;

    ~AggregatedMetrics();

    int32_t native_library_resident_kb() const {
      return aggregated_metrics_->native_library_resident_kb;
    }

    int32_t native_library_resident_not_ordered_kb() const {
      return aggregated_metrics_->native_library_resident_not_ordered_kb;
    }

    int32_t native_library_not_resident_ordered_kb() const {
      return aggregated_metrics_->native_library_not_resident_ordered_kb;
    }

    static constexpr int32_t kInvalid = -1;

   private:
    const mojom::AggregatedMetricsPtr aggregated_metrics_;
  };

  GlobalMemoryDump(const GlobalMemoryDump&) = delete;
  GlobalMemoryDump& operator=(const GlobalMemoryDump&) = delete;

  ~GlobalMemoryDump();

  // Creates an owned instance of this class wrapping the given mojo struct.
  static std::unique_ptr<GlobalMemoryDump> MoveFrom(
      mojom::GlobalMemoryDumpPtr ptr);

  const std::forward_list<ProcessDump>& process_dumps() const {
    return process_dumps_;
  }

  const AggregatedMetrics& aggregated_metrics() { return aggregated_metrics_; }

 private:
  GlobalMemoryDump(std::vector<mojom::ProcessMemoryDumpPtr> process_dumps,
                   mojom::AggregatedMetricsPtr aggregated_metrics);

  std::forward_list<ProcessDump> process_dumps_;
  AggregatedMetrics aggregated_metrics_;
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_GLOBAL_MEMORY_DUMP_H_
