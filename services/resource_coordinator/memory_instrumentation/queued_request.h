// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_QUEUED_REQUEST_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_QUEUED_REQUEST_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

using base::trace_event::MemoryDumpDeterminism;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::MemoryDumpType;

namespace memory_instrumentation {

using OSMemDumpMap =
    base::flat_map<base::ProcessId,
                   memory_instrumentation::mojom::RawOSMemDumpPtr>;

// Holds data for pending requests enqueued via RequestGlobalMemoryDump().
struct QueuedRequest {
  using RequestGlobalMemoryDumpInternalCallback = base::OnceCallback<
      void(bool, uint64_t, memory_instrumentation::mojom::GlobalMemoryDumpPtr)>;

  struct Args {
    Args(MemoryDumpType dump_type,
         MemoryDumpLevelOfDetail level_of_detail,
         MemoryDumpDeterminism determinism,
         const std::vector<std::string>& allocator_dump_names,
         bool add_to_trace,
         base::ProcessId pid,
         bool memory_footprint_only);
    Args(const Args&);
    ~Args();

    const MemoryDumpType dump_type;
    const MemoryDumpLevelOfDetail level_of_detail;
    const MemoryDumpDeterminism determinism;
    const std::vector<std::string> allocator_dump_names;
    const bool add_to_trace;
    const base::ProcessId pid;

    // If this member is |true|, then no MemoryDumpProviders are queried. The
    // only other relevant member is |pid|.
    const bool memory_footprint_only;
  };

  struct PendingResponse {
    enum Type {
      kChromeDump,
      kOSDump,
    };
    PendingResponse(base::ProcessId, const Type type);

    bool operator<(const PendingResponse& other) const;

    const base::ProcessId process_id;
    const Type type;
  };

  struct Response {
    Response();
    Response(Response&& other);
    ~Response();

    base::ProcessId process_id = base::kNullProcessId;
    mojom::ProcessType process_type = mojom::ProcessType::OTHER;
    base::Optional<std::string> service_name;
    std::unique_ptr<base::trace_event::ProcessMemoryDump> chrome_dump;
    OSMemDumpMap os_dumps;
  };

  QueuedRequest(const Args& args,
                uint64_t dump_guid,
                RequestGlobalMemoryDumpInternalCallback callback);
  ~QueuedRequest();

  base::trace_event::MemoryDumpRequestArgs GetRequestArgs();

  mojom::MemoryMapOption memory_map_option() const {
    return args.level_of_detail ==
                   base::trace_event::MemoryDumpLevelOfDetail::DETAILED
               ? mojom::MemoryMapOption::FULL
               : mojom::MemoryMapOption::NONE;
  }

  bool should_return_summaries() const {
    return args.dump_type == base::trace_event::MemoryDumpType::SUMMARY_ONLY;
  }

  const Args args;
  const uint64_t dump_guid;
  RequestGlobalMemoryDumpInternalCallback callback;

  // When a dump, requested via RequestGlobalMemoryDump(), is in progress this
  // set contains a |PendingResponse| for each |RequestChromeMemoryDump| and
  // |RequestOSMemoryDump| call that has not yet replied or been canceled (due
  // to the client disconnecting).
  std::set<PendingResponse> pending_responses;
  std::map<base::ProcessId, Response> responses;
  int failed_memory_dump_count = 0;
  bool dump_in_progress = false;

  // This field is set to |true| before a heap dump is requested, and set to
  // |false| after the heap dump has been added to the trace.
  bool heap_dump_in_progress = false;

  // The time we started handling the request (does not including queuing
  // time).
  base::TimeTicks start_time;
};

// Holds data for pending requests enqueued via GetVmRegionsForHeapProfiler().
struct QueuedVmRegionRequest {
  QueuedVmRegionRequest(
      uint64_t dump_guid,
      mojom::HeapProfilerHelper::GetVmRegionsForHeapProfilerCallback callback);
  ~QueuedVmRegionRequest();
  const uint64_t dump_guid;
  mojom::HeapProfilerHelper::GetVmRegionsForHeapProfilerCallback callback;

  struct Response {
    Response();
    ~Response();

    base::ProcessId process_id = base::kNullProcessId;
    OSMemDumpMap os_dumps;
    base::Optional<std::string> service_name;
  };

  std::set<base::ProcessId> pending_responses;
  std::map<base::ProcessId, Response> responses;
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_QUEUED_REQUEST_H_
