// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_GLOBAL_MEMORY_DUMP_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_GLOBAL_MEMORY_DUMP_H_

#include "base/component_export.h"
#include "base/optional.h"
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
    ~ProcessDump();

    // Returns the metric for the given dump name and metric name. For example,
    // GetMetric("blink", "size") would return the aggregated sze of the
    // "blink/" dump.
    base::Optional<uint64_t> GetMetric(const std::string& dump_name,
                                       const std::string& metric_name) const;

    base::ProcessId pid() const { return raw_dump_->pid; }
    mojom::ProcessType process_type() const { return raw_dump_->process_type; }

    const mojom::OSMemDump& os_dump() const { return *raw_dump_->os_dump; }

   private:
    mojom::ProcessMemoryDumpPtr raw_dump_;

    DISALLOW_COPY_AND_ASSIGN(ProcessDump);
  };

  ~GlobalMemoryDump();

  // Creates an owned instance of this class wrapping the given mojo struct.
  static std::unique_ptr<GlobalMemoryDump> MoveFrom(
      mojom::GlobalMemoryDumpPtr ptr);

  const std::forward_list<ProcessDump>& process_dumps() const {
    return process_dumps_;
  }

 private:
  GlobalMemoryDump(std::vector<mojom::ProcessMemoryDumpPtr> process_dumps);

  std::forward_list<ProcessDump> process_dumps_;

  DISALLOW_COPY_AND_ASSIGN(GlobalMemoryDump);
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_GLOBAL_MEMORY_DUMP_H_
